#include "dataio/UDPSocketDataIO.h"
#include "iogateway/PacketTunnelIOGateway.h"
#include "util/NetworkUtilityFunctions.h"

#include "zg/ZGConstants.h"
#include "zg/private/PZGConstants.h"
#include "zg/private/PZGNetworkIOSession.h"

namespace zg_private
{

enum {
   PZG_NETWORK_COMMAND_SET_SENIOR_PEER_ID = 1886283124, // 'pnet'
   PZG_NETWORK_COMMAND_SET_BEACON_DATA,
   PZG_NETWORK_COMMAND_INVALIDATE_LAST_RECEIVED_BEACON_DATA
};

static const String PZG_NETWORK_NAME_PEER_ID           = "pid";
static const String PZG_NETWORK_NAME_BEACON_DATA       = "bcd";
static const String PZG_NETWORK_NAME_DATABASE_UPDATE   = "dbu";
static const String PZG_NETWORK_NAME_MULTICAST_MESSAGE = "mms";
static const String PZG_NETWORK_NAME_MULTICAST_TAG     = "mgt";

enum {
   PZG_MULTICAST_MESSAGE_TAG_TYPE = 1886219636 // 'pmmt'
};

// Convenience class to hold both a ZGPeerID and a Message-ID in a single object
// We use this to de-duplicate multicasted Messages so the higher levels don't have
// to deal with multiple copies of them on hosts with multiple network interfaces.
class PZGMulticastMessageTag : public PseudoFlattenable<PZGMulticastMessageTag>
{
public:
   PZGMulticastMessageTag() : _versionCode(0), _messageID(0) {/* empty */}
   PZGMulticastMessageTag(const ZGPeerID & peerID, uint32 versionCode, uint32 messageID) : _peerID(peerID), _versionCode(versionCode), _messageID(messageID) {/* empty */}
   PZGMulticastMessageTag(const PZGMulticastMessageTag & rhs)
   {
      _peerID      = rhs._peerID;
      _versionCode = rhs._versionCode;
      _messageID   = rhs._messageID;
   }

   const ZGPeerID & GetPeerID() const {return _peerID;}
   uint32 GetCompatibilityVersionCode() const {return _versionCode;}
   uint32 GetMessageID() const {return _messageID;}

   /** Returns a String representation of this tag ID (e.g. "123A:432B.11420000.12345") */
   String ToString() const
   {
      char buf[256]; muscleSprintf(buf, "." XINT32_FORMAT_SPEC "." UINT32_FORMAT_SPEC, _versionCode, _messageID);
      return _peerID.ToString()+buf;
   }

   bool operator == (const PZGMulticastMessageTag & rhs) const {return ((_peerID == rhs._peerID)&&(_versionCode == rhs._versionCode)&&(_messageID == rhs._messageID));}
   bool operator != (const PZGMulticastMessageTag & rhs) const {return !(*this==rhs);}

   PZGMulticastMessageTag & operator = (const PZGMulticastMessageTag & rhs)
   {
      _peerID      = rhs._peerID;
      _versionCode = rhs._versionCode;
      _messageID   = rhs._messageID;
      return *this;
   }

   static bool IsFixedSize()        {return true;}
   static uint32 TypeCode()         {return PZG_MULTICAST_MESSAGE_TAG_TYPE;}
   static uint32 FlattenedSize()    {return ZGPeerID::FlattenedSize() + sizeof(uint32) + sizeof(uint32);}
   uint32 CalculateChecksum() const {return _peerID.CalculateChecksum()+_versionCode+_messageID;}

   void Flatten(DataFlattener flat) const
   {
      flat.WriteFlat(_peerID);
      flat.WriteInt32(_versionCode);
      flat.WriteInt32(_messageID);
   }

   status_t Unflatten(DataUnflattener & unflat)
   {
      MRETURN_ON_ERROR(unflat.ReadFlat(_peerID));
      _versionCode = unflat.ReadInt32();
      _messageID   = unflat.ReadInt32();
      return unflat.GetStatus();
   }

   uint32 HashCode() const {return CalculateChecksum();}

   bool IsValid() const {return _peerID.IsValid();}

private:
   ZGPeerID _peerID;
   uint32 _versionCode;  // M.mm.bb.uu
   uint32 _messageID;
};

class PZGUnicastSessionFactory : public ReflectSessionFactory
{
public:
   PZGUnicastSessionFactory(PZGNetworkIOSession * master) : _master(master) {/* empty */}

   virtual AbstractReflectSessionRef CreateSession(const String & /*clientAddress*/, const IPAddressAndPort & /*factoryInfo*/)
   {
      return AbstractReflectSessionRef(new PZGUnicastSession(_master, ZGPeerID()));
   }

private:
   PZGNetworkIOSession * _master;
};
DECLARE_REFTYPES(PZGUnicastSessionFactory);

// serializeBeaconData controls whether we add the beaconData to the Message in flattened-to-bytes form
// or just add the PZGBeaconData object to the Message.  Logically it's the same but efficiency can be
// better one way or the other, depending on what we are going to do with the Message next.
static MessageRef CreateBeaconDataMessage(const ConstPZGBeaconDataRef & beaconData, bool serializeBeaconData, const PZGMulticastMessageTag & sourceTag)
{
   MessageRef msg = GetMessageFromPool(PZG_NETWORK_COMMAND_SET_BEACON_DATA);
   if (msg() == NULL) return MessageRef();

   if (beaconData())
   {
      if (serializeBeaconData)
      {
         if (msg()->AddFlat(PZG_NETWORK_NAME_BEACON_DATA, CastAwayConstFromRef(beaconData)).IsError()) return MessageRef();
      }
      else if (msg()->AddFlat(PZG_NETWORK_NAME_BEACON_DATA, *beaconData()).IsError()) return MessageRef();
   }

   if ((sourceTag.IsValid())&&(msg()->AddFlat(PZG_NETWORK_NAME_MULTICAST_TAG, sourceTag).IsError())) return MessageRef();

   return msg;
}

// Given a Message previously created by CreateBeaconDataMessage(), tries to
// retrieve and return the PZGBeaconDataRef from the Message.  Returns NULL ref on failure.
static ConstPZGBeaconDataRef GetBeaconDataFromMessage(const MessageRef & msg)
{
   PZGBeaconDataRef beaconRef;
   if (msg()->FindFlat(PZG_NETWORK_NAME_BEACON_DATA, beaconRef).IsOK()) return AddConstToRef(beaconRef);

   // Didn't work?  Okay, let's try to unflatten some bytes instead
   beaconRef = GetBeaconDataFromPool();
   if ((beaconRef())&&(msg()->FindFlat(PZG_NETWORK_NAME_BEACON_DATA, *beaconRef()).IsError())) beaconRef.Reset();
   return AddConstToRef(beaconRef);
}

PZGNetworkIOSession :: PZGNetworkIOSession(const ZGPeerSettings & peerSettings, const ZGPeerID & localPeerID, ZGPeerSession * master)
   : _peerSettings(peerSettings)
   , _localPeerID(localPeerID)
   , _beaconIntervalMicros(SecondsToMicros(1)/muscleMax((uint32)1, peerSettings.GetBeaconsPerSecond()))
   , _master(master)
   , _computerIsAsleep(false)
   , _hbSessionPtr(NULL)
{
   (void) SetThreadPriority(PRIORITY_HIGH);
}

void PZGNetworkIOSession :: MessageReceivedFromInternalThread(const MessageRef & msg, uint32 /*numLeft*/)
{
   PZGMulticastMessageTag tag;
   if (msg()->FindFlat(PZG_NETWORK_NAME_MULTICAST_TAG, tag).IsOK())
   {
      if (msg()->what == PZG_NETWORK_COMMAND_SET_BEACON_DATA)
      {
         if (tag.GetPeerID() == _seniorPeerID)
         {
            ConstPZGBeaconDataRef beaconData = GetBeaconDataFromMessage(msg);
            if (beaconData()) _master->BeaconDataChanged(beaconData);
                         else LogTime(MUSCLE_LOG_ERROR, "PZGNetworkIOSession:  Unable to get beacon data from internal thread Message\n");
         }
         else
         {
            // Hmm, race condition caused us to end up with beacon data from the wrong senior peer?
            // we'd better as the I/O thread to try again
            static Message _retryMsg(PZG_NETWORK_COMMAND_INVALIDATE_LAST_RECEIVED_BEACON_DATA);
            if (SendMessageToInternalThread(DummyMessageRef(_retryMsg)).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send message to invalidate last received beacon data!\n");
         }
      }
      else _master->PrivateMessageReceivedFromPeer(tag.GetPeerID(), msg);
   }
}

status_t PZGNetworkIOSession :: AttachedToServer()
{
   PZGUnicastSessionFactoryRef unicastTCPFactoryRef(new PZGUnicastSessionFactory(this));

   uint16 tcpAcceptPort;
   MRETURN_ON_ERROR(PutAcceptFactory(0, unicastTCPFactoryRef, invalidIP, &tcpAcceptPort));

   _hbSettings.SetRef(new PZGHeartbeatSettings(_peerSettings, _localPeerID, tcpAcceptPort));
   LogTime(MUSCLE_LOG_DEBUG, "This peer's ZGPeerID is:  [%s]\n", GetLocalPeerID().ToString()());

   DetectNetworkConfigChangesSessionRef dnccSessionRef(new DetectNetworkConfigChangesSession);

   status_t ret;
   if (AddNewSession(dnccSessionRef).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGNetworkIOSession::AttachedToServer():  Couldn't add DetectNetworkConfigChangesSession! [%s]\n", ret());
      ShutdownChildSessions();
      return ret;
   }
   _dnccSession = dnccSessionRef;

   if (SetupHeartbeatSession().IsError(ret)) {ShutdownChildSessions(); return ret;}

   return PZGThreadedSession::AttachedToServer();
}

status_t PZGNetworkIOSession :: SetupHeartbeatSession()
{
   PZGHeartbeatSessionRef hbSessionRef(new PZGHeartbeatSession(_hbSettings, this));

   status_t ret;
   if (AddNewSession(hbSessionRef).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGNetworkIOSession::AttachedToServer():  Couldn't add heartbeat session!\n");
      return ret;
   }
   _hbSession = hbSessionRef;

   DECLARE_MUTEXGUARD(_hbSessionPtrMutex);
   _hbSessionPtr = _hbSession();
   return B_NO_ERROR;
}

void PZGNetworkIOSession :: AboutToDetachFromServer()
{
   ShutdownChildSessions();
   PZGThreadedSession::AboutToDetachFromServer();
}

void PZGNetworkIOSession :: EndSession()
{
   PZGThreadedSession::EndSession();
   ShutdownChildSessions();
}

void PZGNetworkIOSession :: ClearHeartbeatSession()
{
   if (_hbSession())
   {
      {
         DECLARE_MUTEXGUARD(_hbSessionPtrMutex);
         _hbSessionPtr = NULL;
      }
      _hbSession()->EndSession();
      _hbSession.Reset();
   }
}

void PZGNetworkIOSession :: ShutdownChildSessions()
{
   if (_hbSettings())
   {
      (void) RemoveAcceptFactory(_hbSettings()->GetDataTCPPort());
      _hbSettings.Reset();
   }

   if (_dnccSession())
   {
      _dnccSession()->EndSession();
      _dnccSession.Reset();
   }

   ClearHeartbeatSession();
   ClearAllUnicastSessions();

   _master = NULL;
}

int64 PZGNetworkIOSession :: GetToNetworkTimeOffset() const
{
   DECLARE_MUTEXGUARD(_hbSessionPtrMutex);  // this is here primary to mollify ThreadSanitizer
   return _hbSessionPtr ? _hbSessionPtr->MainThreadGetToNetworkTimeOffset() : 0;
}

void PZGNetworkIOSession :: ClearAllUnicastSessions()
{
   for (HashtableIterator<PZGUnicastSessionRef, Void> iter(_registeredUnicastSessions); iter.HasData(); iter++) iter.GetKey()()->EndSession();
   _registeredUnicastSessions.Clear();   // these Clear() calls shouldn't be necessary, since EndSession() should remove everything anyway
   _namedUnicastSessions.Clear();        // but for paranoia's sake I'm leaving them in
}

void PZGNetworkIOSession :: RegisterUnicastSession(PZGUnicastSession * s)
{
   PZGUnicastSessionRef sRef(s);

   const ZGPeerID & remotePeerID = s->GetRemotePeerID();
   (void) _registeredUnicastSessions.PutWithDefault(sRef);
   if (remotePeerID.IsValid())
   {
      Queue<PZGUnicastSessionRef> * q = _namedUnicastSessions.GetOrPut(remotePeerID);
      if (q) (void) q->AddTail(sRef);
   }
}

void PZGNetworkIOSession :: UnregisterUnicastSession(PZGUnicastSession * s)
{
   PZGUnicastSessionRef sRef(s);

   const ZGPeerID & remotePeerID = s->GetRemotePeerID();
   (void) _registeredUnicastSessions.Remove(sRef);
   if (remotePeerID.IsValid())
   {
      Queue<PZGUnicastSessionRef> * q = _namedUnicastSessions.Get(remotePeerID);
      if ((q)&&(q->RemoveAllInstancesOf(sRef) > 0)&&(q->IsEmpty())) (void) _namedUnicastSessions.Remove(remotePeerID);
   }
}

void PZGNetworkIOSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   LogTime(MUSCLE_LOG_INFO, "Peer [%s] has come online (" UINT32_FORMAT_SPEC " sources, %s)\n", peerID.ToString()(), GetMainThreadPeers()[peerID].GetNumItems(), PeerInfoToString(peerInfo)());
   if (_master) _master->PeerHasComeOnline(peerID, peerInfo);
}

void PZGNetworkIOSession :: PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   LogTime(MUSCLE_LOG_INFO, "Peer [%s] has gone offline (%s)\n", peerID.ToString()(), PeerInfoToString(peerInfo)());
   if (_master) _master->PeerHasGoneOffline(peerID, peerInfo);

   // Since the peer is gone, we'll assume that any TCP connections associated with that peer are now
   // moribund as well, and encourage them to go away sooner rather than later
   Queue<PZGUnicastSessionRef> q = _namedUnicastSessions[peerID];  // I'm making a copy here just to avoid possible re-entrancy issues
   for (uint32 i=0; i<q.GetNumItems(); i++) q[i]()->EndSession();  // tell the session to unregister itself and go away
}

void PZGNetworkIOSession :: SeniorPeerChanged(const ZGPeerID & oldSeniorPeerID, const ZGPeerID & newSeniorPeerID)
{
   if (newSeniorPeerID != _seniorPeerID)
   {
      _seniorPeerID = newSeniorPeerID;
      if (_master) _master->SeniorPeerChanged(oldSeniorPeerID, newSeniorPeerID);

      // Let's tell the internal thread what the senior peer ID is too, so he
      // can know which beacon datas to send us and which to ignore
      MessageRef msg = GetMessageFromPool(PZG_NETWORK_COMMAND_SET_SENIOR_PEER_ID);
      if ((msg() == NULL)||(msg()->AddFlat(PZG_NETWORK_NAME_PEER_ID, _seniorPeerID).IsError())||(SendMessageToInternalThread(msg).IsError())) LogTime(MUSCLE_LOG_ERROR, "PZGNetworkSession::SeniorPeerChanged:  Couldn't inform multicast thread!\n");
   }
}

void PZGNetworkIOSession :: InternalThreadEntry()
{
   // multicast I/O for data payloads will go here
   bool recreateMulticastDataIORequested = true;

   uint32 outgoingMulticastMessageTagCounter = 0; // tagging our outgoing Messages with a unique ID allows us to do de-duplication more easily
   Queue<PacketDataIORef> dios;
   Queue<PacketTunnelIOGatewayRef> ptGateways; // our mechanism for transporting Message objects by packing them into UDP packets
   QueueGatewayMessageReceiver messageReceiver;   // a place that the ptGateways can store incoming/received Messages for us to collect
   Hashtable<PZGMulticastMessageTag, Void> recentlyReceived;  // PZGMulticastMessageTags that we have received recently

   ZGPeerID seniorPeerID;
   MessageRef outgoingBeaconMsg;
   ConstPZGBeaconDataRef outgoingBeaconData;     // should be non-NULL only when when we are the senior peer
   ConstPZGBeaconDataRef lastReceivedBeaconData;
   uint64 nextBeaconSendTime = MUSCLE_TIME_NEVER;

   // Multicast-data I/O thread's main event loop
   while(1)
   {
      // Demand-allocate our multicast DataIO
      if (recreateMulticastDataIORequested)
      {
         recreateMulticastDataIORequested = false;

         // Get rid of any old DataIO
         for (uint32 i=0; i<dios.GetNumItems(); i++)
         {
            PacketDataIORef & dio = dios[i];
            (void) UnregisterInternalThreadSocket(dio()->GetReadSelectSocket(),  SOCKET_SET_READ);
            (void) UnregisterInternalThreadSocket(dio()->GetWriteSelectSocket(), SOCKET_SET_WRITE);
         }
         dios.Clear();
         ptGateways.Clear();

         // Install the new DataIO
         dios = _hbSettings()->CreateMulticastDataIOs(false, GetNetworkInterfaceFilter());
         if (dios.HasItems())
         {
            for (uint32 i=0; i<dios.GetNumItems(); i++)
            {
               PacketDataIORef & dio = dios[i];
               if (RegisterInternalThreadSocket(dio()->GetReadSelectSocket(), SOCKET_SET_READ).IsError()) LogTime(MUSCLE_LOG_ERROR, "PZGNetworkIOSession:  Couldn't register DataIO # " UINT32_FORMAT_SPEC " for input!\n", i);

               PacketTunnelIOGatewayRef ptRef(new PacketTunnelIOGateway);
               if (ptGateways.AddTail(ptRef).IsOK()) ptRef()->SetDataIO(dio);
            }
         }
         else LogTime(MUSCLE_LOG_ERROR, "PZGNetworkIOSession:  Couldn't create Multicast DataIOs!\n");
      }

      // Figure out if we need to wake up as soon as we can send data, or not
      for (uint32 i=0; i<dios.GetNumItems(); i++)
      {
         PacketDataIO & dio = *dios[i]();  // guaranteed non-NULL
         const bool hasBytesToOutput = ptGateways[i]()->HasBytesToOutput();
         if (hasBytesToOutput) (void)   RegisterInternalThreadSocket(dio.GetWriteSelectSocket(), SOCKET_SET_WRITE);
                          else (void) UnregisterInternalThreadSocket(dio.GetWriteSelectSocket(), SOCKET_SET_WRITE);
      }

      // Wait until there is data to receive (or until there is buffer space to send, if we need to send anything), or until we get a Message from the main thread
      MessageRef msgFromOwner;
      if (WaitForNextMessageFromOwner(msgFromOwner, nextBeaconSendTime).IsOK())
      {
         if (msgFromOwner())
         {
            switch(msgFromOwner()->what)
            {
               case PZG_THREADED_SESSION_RECREATE_SOCKETS:
               {
                  LogTime(MUSCLE_LOG_DEBUG, "Network I/O multicast thread:  forcing recreation of sockets, in response to network configuration change\n");
                  recreateMulticastDataIORequested = true;
               }
               break;

               case PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE: case PZG_PEER_COMMAND_USER_MESSAGE:
                  if (msgFromOwner()->AddFlat(PZG_NETWORK_NAME_MULTICAST_TAG, PZGMulticastMessageTag(GetLocalPeerID(), _hbSettings()->GetCompatibilityVersionCode(), ++outgoingMulticastMessageTagCounter)).IsOK())
                  {
                     for (uint32 i=0; i<ptGateways.GetNumItems(); i++)
                        (void) ptGateways[i]()->AddOutgoingMessage(msgFromOwner);
                  }
               break;

               case PZG_NETWORK_COMMAND_SET_SENIOR_PEER_ID:
                  if (msgFromOwner()->FindFlat(PZG_NETWORK_NAME_PEER_ID, seniorPeerID).IsError()) LogTime(MUSCLE_LOG_ERROR, "Multicast I/O thread:  Couldn't get senior peer ID from Message!\n");
               break;

               case PZG_NETWORK_COMMAND_SET_BEACON_DATA:
               {
                  outgoingBeaconMsg.Reset();  // will be demand-allocated next time we send
                  outgoingBeaconData = GetBeaconDataFromMessage(msgFromOwner);
                  nextBeaconSendTime = (outgoingBeaconData() == NULL) ? MUSCLE_TIME_NEVER : 0;
               }
               break;

               case PZG_NETWORK_COMMAND_INVALIDATE_LAST_RECEIVED_BEACON_DATA:
                  lastReceivedBeaconData.Reset();  // so that we'll resend to the owner thread when that happens
               break;

               default:
                  LogTime(MUSCLE_LOG_ERROR, "Network I/O multicast thread:  Unknown outgoing Message code " UINT32_FORMAT_SPEC "\n", msgFromOwner()->what);
               break;
            }
         }
         else break;  // NULL msgFromOwner means it is time for this thread to exit!
      }

      const uint64 now = GetRunTime64();
      if (now >= nextBeaconSendTime)
      {
         if (outgoingBeaconData())
         {
            nextBeaconSendTime = now + _beaconIntervalMicros;
            if ((_computerIsAsleep == false)&&(_master->IAmFullyAttached()))
            {
               // demand-construct a Beacon Message (and cache it so we don't have to do it again every time)
               // The tag will be handled specially by the receiver so it's okay that the tag ID isn't increasing with each send
               if (outgoingBeaconMsg() == NULL) outgoingBeaconMsg = CreateBeaconDataMessage(outgoingBeaconData, true, PZGMulticastMessageTag(GetLocalPeerID(), _hbSettings()->GetCompatibilityVersionCode(), 0));
               if (outgoingBeaconMsg())
               {
                  for (uint32 i=0; i<ptGateways.GetNumItems(); i++) if (ptGateways[i]()->AddOutgoingMessage(outgoingBeaconMsg).IsError()) LogTime(MUSCLE_LOG_ERROR, "Unable to add outgoing beacon to gateway # " UINT32_FORMAT_SPEC "!\n", i);
               }
               else LogTime(MUSCLE_LOG_ERROR, "Unable to create Outgoing Beacon Message!\n");
            }
         }
         else nextBeaconSendTime = MUSCLE_TIME_NEVER;
      }

      for (uint32 i=0; i<dios.GetNumItems(); i++)
      {
         PacketDataIORef & dio = dios[i];
         if (IsInternalThreadSocketReady(dio()->GetReadSelectSocket(), SOCKET_SET_READ))
         {
            // Read incoming multicast data
            while(ptGateways[i]()->DoInput(messageReceiver).GetByteCount() > 0)
            {
               MessageRef msg;
               while(messageReceiver.RemoveHead(msg).IsOK())
               {
                  // no point in forwarding-to-owner a dup Message, or a Message that came from us, or a Message from an incompatibile peer
                  PZGMulticastMessageTag tag;
                  if ((msg()->FindFlat(PZG_NETWORK_NAME_MULTICAST_TAG, tag).IsOK())&&(tag.GetCompatibilityVersionCode() == _hbSettings()->GetCompatibilityVersionCode())&&(tag.GetPeerID() != GetLocalPeerID())&&((msg()->what == PZG_NETWORK_COMMAND_SET_BEACON_DATA)||((recentlyReceived.ContainsKey(tag) == false)&&(recentlyReceived.PutWithDefault(tag).IsOK()))))
                  {
                     if (msg()->what == PZG_NETWORK_COMMAND_SET_BEACON_DATA)
                     {
                        if (seniorPeerID.IsValid())  // no point trying to handle beacon data until we know who the senior peer is!
                        {
                           if (tag.GetPeerID() == seniorPeerID)
                           {
                              ConstPZGBeaconDataRef incomingBeaconData = GetBeaconDataFromMessage(msg);
                              if (incomingBeaconData())
                              {
                                 // we'll only notify the main thread if the beacon data actually changed
                                 if ((lastReceivedBeaconData() == NULL)||(*incomingBeaconData() != *lastReceivedBeaconData()))
                                 {
                                    lastReceivedBeaconData = incomingBeaconData;
                                    if (SendMessageToOwner(CreateBeaconDataMessage(incomingBeaconData, false, tag)).IsError()) LogTime(MUSCLE_LOG_ERROR, "Multicast thread:  Unable to send beacon data to main thread!\n");
                                 }
                              }
                              else LogTime(MUSCLE_LOG_ERROR, "Multicast thread:  Unable to retrieve beacon data from incoming multicast Message!\n");
                           }
                           else if (_master->IAmFullyAttached()) LogTime(MUSCLE_LOG_WARNING, "Multicast thread received beacon data from peer [%s], but peer [%s] is the senior peer.  Multiple senior peers present?\n", tag.GetPeerID().ToString()(), seniorPeerID.ToString()());
                        }
                     }
                     else
                     {
                        (void) recentlyReceived.MoveToBack(tag);  // might as well use the full LRU semantics
                        if (SendMessageToOwner(msg).IsError()) LogTime(MUSCLE_LOG_ERROR, "Multicast thread:  Unable to send Message to main thread!\n");
                        while(recentlyReceived.GetNumItems() > 1000) (void) recentlyReceived.RemoveFirst();  // don't let our cache get too large
                     }
                  }
               }
            }
         }

         if (IsInternalThreadSocketReady(dio()->GetWriteSelectSocket(), SOCKET_SET_WRITE))
            while(ptGateways[i]()->DoOutput().GetByteCount() > 0) {/* empty */} // Write outgoing multicast data
      }
   }
}

uint64 PZGNetworkIOSession :: GetPulseTime(const PulseArgs & args)
{
   return _messagesSentToSelf.HasItems() ? 0 : PZGThreadedSession::GetPulseTime(args);
}

void PZGNetworkIOSession :: Pulse(const PulseArgs & args)
{
   PZGThreadedSession::Pulse(args);

   ConstMessageRef nextMsgToSelf;
   while(_messagesSentToSelf.RemoveHead(nextMsgToSelf).IsOK()) UnicastMessageReceivedFromPeer(GetLocalPeerID(), CastAwayConstFromRef(nextMsgToSelf));
}

status_t PZGNetworkIOSession :: SendUnicastMessageToAllPeers(const ConstMessageRef & msg, bool sendToSelf)
{
   for (HashtableIterator<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > iter(GetMainThreadPeers()); iter.HasData(); iter++)
   {
      if ((sendToSelf == false)&&(iter.GetKey() == GetLocalPeerID())) continue;
      MRETURN_ON_ERROR(SendUnicastMessageToPeer(iter.GetKey(), msg));
   }
   return B_NO_ERROR;
}

status_t PZGNetworkIOSession :: SendUnicastMessageToPeer(const ZGPeerID & peerID, const ConstMessageRef & msg)
{
   if (_hbSettings() == NULL) return B_BAD_OBJECT;  // paranoia

   if (peerID == GetLocalPeerID())
   {
      // We'll process this Message-to-ourself asynchronously just to avoid any surprises to the calling code,
      // which probably isn't expecting SendUnicastMessageToPeer() to execute any message-handling code during this call!
      const bool wasEmpty = _messagesSentToSelf.IsEmpty();
      const status_t ret  = _messagesSentToSelf.AddTail(msg);
      if ((wasEmpty)&&(ret.IsOK())) InvalidatePulseTime();  // so we'll wake up and receive our message to ourself ASAP
      return ret;
   }
   else
   {
      PZGUnicastSessionRef usRef = GetUnicastSessionForPeerID(peerID, true);
      return usRef() ? usRef()->AddOutgoingMessage(CastAwayConstFromRef(msg)) : B_DATA_NOT_FOUND;
   }
}

status_t PZGNetworkIOSession :: SetBeaconData(const ConstPZGBeaconDataRef & optBeaconData)
{
   MessageRef msg = CreateBeaconDataMessage(optBeaconData, false, PZGMulticastMessageTag());
   return msg() ? SendMessageToInternalThread(msg) : B_OUT_OF_MEMORY;
}

PZGUnicastSessionRef PZGNetworkIOSession :: GetUnicastSessionForPeerID(const ZGPeerID & peerID, bool allocIfNecessary)
{
   const Queue<PZGUnicastSessionRef> * q = _namedUnicastSessions.Get(peerID);
   if ((q)&&(q->HasItems())) return q->Head();  // the easy case; just use a session we already have on hand
   if (allocIfNecessary == false) return B_DATA_NOT_FOUND;

   // If we got here, we're going to have to create one
   IPAddressAndPort iap = GetUnicastIPAddressAndPortForPeerID(peerID);
   if (iap.IsValid() == false)
   {
      LogTime(MUSCLE_LOG_ERROR, "GetUnicastSessionForPeerID():  Couldn't find IP address for peer [%s]!\n", peerID.ToString()());
      return B_DATA_NOT_FOUND;
   }

   PZGUnicastSessionRef usRef(new PZGUnicastSession(this, peerID));

   status_t ret;
   if (AddNewConnectSession(usRef, iap, MUSCLE_TIME_NEVER, SecondsToMicros(5)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "GetUnicastSessionForPeerID():  Couldn't connect to peer [%s] at [%s]! [%s]\n", peerID.ToString()(), iap.ToString()(), ret());
      return ret;
   }

   // the session will register itself with us, so we don't need to explicitly register it here
   return usRef;
}

IPAddressAndPort PZGNetworkIOSession :: GetUnicastIPAddressAndPortForPeerID(const ZGPeerID & peerID, uint32 index) const
{
   const Queue<ConstPZGHeartbeatPacketWithMetaDataRef> * packetRefQ = GetMainThreadPeers().Get(peerID);
   if ((packetRefQ == NULL)||(index >= packetRefQ->GetNumItems())) return IPAddressAndPort();

   const PZGHeartbeatPacketWithMetaData & r = *(*packetRefQ)[index]();
   return IPAddressAndPort(r.GetPacketSource().GetIPAddress(), r.GetTCPAcceptPort());
}

void PZGNetworkIOSession :: NetworkInterfacesChanged(const Hashtable<String, Void> & /*optInterfaceNames*/)
{
   // TODO:  Remove unicast sessions that were using the specified interface(s)?

   (void) TellInternalThreadToRecreateMulticastSockets();
   if (_hbSession()) (void) _hbSession()->TellInternalThreadToRecreateMulticastSockets();
}

void PZGNetworkIOSession :: ComputerIsAboutToSleep()
{
   // Might as well disconnect all unicast-TCP connections cleanly now, rather than leaving them to go stale while we sleep
   ClearHeartbeatSession();
   ClearAllUnicastSessions();

   // because we don't know who it will be when we re-awake
   SeniorPeerChanged(_seniorPeerID, ZGPeerID());

   // Since when we wake up we won't know who is online anymore
   for (HashtableIterator<ZGPeerID, ConstMessageRef> iter(_master->GetOnlinePeers()); iter.HasData(); iter++)
   {
      const ZGPeerID temp = iter.GetKey();  // copy this out first to avoid re-entrancy problems
      PeerHasGoneOffline(temp, iter.GetValue());
   }

   _computerIsAsleep = true;  // because apparently we stay awake for a while even when we're asleep!?  MacOS/X is weird
}

void PZGNetworkIOSession :: ComputerJustWokeUp()
{
   _computerIsAsleep = false;
   if (SetupHeartbeatSession().IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't recreate heartbeat session!\n");
}

void PZGNetworkIOSession :: UnicastMessageReceivedFromPeer(const ZGPeerID & remotePeerID, const MessageRef & msg)
{
   _master->PrivateMessageReceivedFromPeer(remotePeerID, msg);
}

status_t PZGNetworkIOSession :: RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok, bool dueToChecksumError)
{
   if (_hbSettings() == NULL) return B_BAD_OBJECT;  // paranoia

   const ZGPeerID & peerID = ubok.GetTargetPeerID();
   if (peerID == GetLocalPeerID())
   {
      // I don't think there is any reason to ever want to do this, so I'm not going to implement it for now --jaf
      LogTime(MUSCLE_LOG_ERROR, "PZGNetworkIOSession::RequestBackOrderFromSeniorPeer:  Requesting a back order from myself isn't implemented, and shouldn't be necessary.\n");
      return B_UNIMPLEMENTED;
   }
   else
   {
      PZGUnicastSessionRef usRef = GetUnicastSessionForPeerID(peerID, true);
      return usRef() ? usRef()->RequestBackOrderFromSeniorPeer(ubok, dueToChecksumError) : B_DATA_NOT_FOUND;
   }
}

void PZGNetworkIOSession :: BackOrderResultReceived(const PZGUpdateBackOrderKey & ubok, const ConstPZGDatabaseUpdateRef & optDBUp)
{
   _master->BackOrderResultReceived(ubok, optDBUp);
}

ConstPZGDatabaseUpdateRef PZGNetworkIOSession :: GetDatabaseUpdateByID(uint32 whichDB, uint64 updateID) const
{
   return _master->GetDatabaseUpdateByID(whichDB, updateID);
}

void PZGNetworkIOSession :: VerifyOrFixLocalDatabaseChecksum(uint32 whichDB)
{
   return _master->VerifyOrFixLocalDatabaseChecksum(whichDB);
}

uint64 PZGNetworkIOSession :: GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const
{
   return _hbSession() ? _hbSession()->GetEstimatedLatencyToPeer(peerID) : MUSCLE_TIME_NEVER;
}

uint16 PZGNetworkIOSession :: GetTimeSyncUDPPort() const
{
   return _hbSession() ? _hbSession()->MainThreadGetTimeSyncUDPPort() : 0;
}

const INetworkInterfaceFilter * PZGNetworkIOSession :: GetNetworkInterfaceFilter() const
{
   return _master;
}

};  // end namespace zg_private

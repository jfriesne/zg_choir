#include "iogateway/MessageIOGateway.h"
#include "util/NetworkUtilityFunctions.h"

#include "zg/ZGConstants.h"
#include "zg/private/PZGCaffeine.h"
#include "zg/private/PZGConstants.h"
#include "zg/private/PZGHeartbeatPacket.h"
#include "zg/private/PZGHeartbeatSession.h"
#include "zg/private/PZGNetworkIOSession.h"
#include "zg/private/PZGHeartbeatSourceState.h"

namespace zg_private
{

enum {
   PZG_HEARTBEAT_COMMAND_PEERS_UPDATE = 1751281781 // 'hbpu'
};

static const String PZG_HEARTBEAT_NAME_PEERINFO = "hpi";
static const String PZG_HEARTBEAT_NAME_PEER_ID  = "pid";

PZGHeartbeatSession :: PZGHeartbeatSession(const ConstPZGHeartbeatSettingsRef & hbSettings, PZGNetworkIOSession * master) : _hbSettings(hbSettings), _master(master), _timeSyncUDPPort(0)
{
   (void) SetThreadPriority(PRIORITY_HIGHER);
}

void PZGHeartbeatSession :: MessageReceivedFromInternalThread(const MessageRef & msgFromHeartbeatThread, uint32 /*numLeft*/)
{
   switch(msgFromHeartbeatThread()->what)
   {
      case PZG_HEARTBEAT_COMMAND_PEERS_UPDATE:
      {
         uint32 numPeers = msgFromHeartbeatThread()->GetNumValuesInName(PZG_HEARTBEAT_NAME_PEERINFO, PZG_HEARTBEAT_PACKET_TYPE_CODE);
         Hashtable<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > newPeers; (void) newPeers.EnsureSize(numPeers);
         for (uint32 i=0; i<numPeers; i++)
         {
            PZGHeartbeatPacketWithMetaDataRef hbRef;
            if (msgFromHeartbeatThread()->FindFlat(PZG_HEARTBEAT_NAME_PEERINFO, i, hbRef).IsOK())
            {
               Queue<ConstPZGHeartbeatPacketWithMetaDataRef> * q = newPeers.GetOrPut(hbRef()->GetSourcePeerID());
               if (q) (void) q->AddTail(hbRef);
            }
         }

         const ZGPeerID oldSeniorPeerID = GetSeniorPeerID();  // deliberately making a copy of the ZGPeerID object on this line

         // First, notify the user-code about any existing peers that have gone away (or changed in such as way that we will treat them as if they had gone away)
         for (HashtableIterator<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > iter(_mainThreadPeers); iter.HasData(); iter++)
         {
            const ZGPeerID & peerID = iter.GetKey();
            const Queue<ConstPZGHeartbeatPacketWithMetaDataRef> & oldInfoQ = iter.GetValue();
            const Queue<ConstPZGHeartbeatPacketWithMetaDataRef> * newInfoQ = newPeers.Get(peerID);
            if ((newInfoQ == NULL)||(newInfoQ->Head()()->IsEqualIgnoreTransients(*oldInfoQ.Head()()) == false))
            {
               ZGPeerID tempPeerID = peerID;  // gotta make a copy here since the Remove() will invalidate the (peerID) reference
               ConstPZGHeartbeatPacketWithMetaDataRef optOldRef = oldInfoQ.Head();  // take a reference to it here to avoid premature deletion below
               (void) _mainThreadPeers.Remove(peerID);  // gotta do this BEFORE calling PeerHasGoneOffline()
               _master->PeerHasGoneOffline(tempPeerID, optOldRef()?optOldRef()->GetPeerAttributesAsMessage():ConstMessageRef());  // yes, tempPeerID is necessary!
            }
         }

         // Then, notify the user-code about any new peers that have appeared
         const ZGPeerID * optPrevID = NULL;
         for (HashtableIterator<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > iter(newPeers); iter.HasData(); iter++)
         {
            const ZGPeerID & peerID = iter.GetKey();
            const Queue<ConstPZGHeartbeatPacketWithMetaDataRef> & newRefQ = iter.GetValue();

            Queue<ConstPZGHeartbeatPacketWithMetaDataRef> * optOldRefQ = _mainThreadPeers.Get(peerID);
            if (optOldRefQ)
            {
               *optOldRefQ = newRefQ;  // might as well make sure we have the latest sources, even if we won't inform the user code about it
            }
            else if ((_mainThreadPeers.Put(peerID, newRefQ).IsOK())&&((optPrevID?_mainThreadPeers.MoveToBehind(peerID, *optPrevID):_mainThreadPeers.MoveToFront(peerID))==B_NO_ERROR))
            {
               _master->PeerHasComeOnline(peerID, newRefQ.Head()()->GetPeerAttributesAsMessage());
            }

            optPrevID = &peerID;
         }

         const ZGPeerID & newSeniorPeerID = GetSeniorPeerID();
         if (newSeniorPeerID != oldSeniorPeerID) _master->SeniorPeerChanged(oldSeniorPeerID, newSeniorPeerID);
      }
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatSession::MessageReceivedFromInternalThread:  Unknown what code " UINT32_FORMAT_SPEC "\n", msgFromHeartbeatThread()->what);
      break;
   }
}

status_t PZGHeartbeatSession :: AttachedToServer()
{
   _timeSyncUDPSocket = CreateUDPSocket();
   if (_timeSyncUDPSocket() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "PZGHeartbeatSession::AttachedToServer():  CreateUDPSocket() failed!\n");
      return B_IO_ERROR;
   }

   // Do this before the superclass call, so that the socket is ready for use when the internal thread starts
   MRETURN_ON_ERROR(BindUDPSocket(_timeSyncUDPSocket, 0, &_timeSyncUDPPort));
   MRETURN_ON_ERROR(SetSocketBlockingEnabled(_timeSyncUDPSocket, false));
   return PZGThreadedSession::AttachedToServer();  // starts the internal thread
}

void PZGHeartbeatSession :: AboutToDetachFromServer()
{
   _master = NULL;
   PZGThreadedSession::AboutToDetachFromServer();
}

void PZGHeartbeatSession :: EndSession()
{
   _master = NULL;
   PZGThreadedSession::EndSession();
}

void PZGHeartbeatSession :: InternalThreadEntry()
{
   const PZGCaffeine caffeine("Sending Heartbeats");  // keep MacOS/X from app-napping on us and causing other peers to think we've gone offline when we don't sent heartbeats on schedule

   _hbtState.Initialize(_hbSettings, GetRunTime64());
   Queue<MessageRef> messagesForOwnerThread;

   ConstSocketRef localTimeSyncUDPSocket = _timeSyncUDPSocket;  // so we can pretend it's gone without affecting the main thread
   if (localTimeSyncUDPSocket()) (void) RegisterInternalThreadSocket(localTimeSyncUDPSocket, SOCKET_SET_READ);

   MessageIOGateway timeSyncGateway;  // we instantiate this solely so we can call its CallUnflattenHeaderAndMessage() and CallFlattenHeaderAndMessage() methods

   while(1)
   {
      // Demand-allocate our multicast DataIO
      if (_hbtState._recreateMulticastDataIOsRequested)
      {
         _hbtState._recreateMulticastDataIOsRequested = false;

         // Get rid of any old DataIOs
         Queue<PacketDataIORef> & dios = _hbtState._multicastDataIOs;
         for (uint32 i=0; i<dios.GetNumItems(); i++) (void) UnregisterInternalThreadSocket(dios[i]()->GetReadSelectSocket(), SOCKET_SET_READ);
         dios.Clear();

         // Install the new DataIOs
         dios = _hbSettings()->CreateMulticastDataIOs(true, _master->GetNetworkInterfaceFilter());
         if (dios.HasItems())
         {
            for (uint32 i=0; i<dios.GetNumItems(); i++)
               if (RegisterInternalThreadSocket(dios[i]()->GetReadSelectSocket(), SOCKET_SET_READ).IsError())
                  LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatSession:  Couldn't register Multicast DataIO #" UINT32_FORMAT_SPEC "!\n", i);
         }
         else LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatSession:  Couldn't create any Multicast DataIOs!\n");
      }

      const uint64 pulseTime = _hbtState.GetPulseTime();
      MessageRef msgFromOwner;
      const status_t waitRet = WaitForNextMessageFromOwner(msgFromOwner, pulseTime);

      if ((localTimeSyncUDPSocket())&&(IsInternalThreadSocketReady(localTimeSyncUDPSocket, SOCKET_SET_READ)))
      {
         const uint64 currentNetworkTime = _hbtState.GetNetworkTime64ForRunTime64(GetRunTime64());

         ByteBuffer inputBB;
         uint8 buf[2048];
         while(true)
         {
            IPAddress fromAddr;
            uint16 fromPort;
            const io_status_t numTimeSyncBytesRead = ReceiveDataUDP(localTimeSyncUDPSocket, buf, sizeof(buf), false, &fromAddr, &fromPort);
            if (numTimeSyncBytesRead.GetByteCount() > 0)
            {
               // Parse the incoming ping-Message
               inputBB.AdoptBuffer(numTimeSyncBytesRead.GetByteCount(), buf);
               MessageRef pingMsg = timeSyncGateway.CallUnflattenHeaderAndMessage(DummyConstByteBufferRef(inputBB));
               if ((pingMsg())&&(pingMsg()->AddInt64("stm", currentNetworkTime).IsOK()))
               {
                  ConstByteBufferRef outputBB = timeSyncGateway.CallFlattenHeaderAndMessage(pingMsg);
                  if (outputBB())
                  {
                     // Send back the corresponding pong-Message with our timestamp added
                     (void) SendDataUDP(localTimeSyncUDPSocket, outputBB()->GetBuffer(), outputBB()->GetNumBytes(), false, fromAddr, fromPort);
                  }
               }
               (void) inputBB.ReleaseBuffer();
            }
            else if (numTimeSyncBytesRead.GetByteCount() == 0) break;  // nothing more to read, for now
            else if (numTimeSyncBytesRead.IsError())
            {
               LogTime(MUSCLE_LOG_CRITICALERROR, "PZGHeartbeatSession:  Time Sync socket had a read-error [%s], abandoning it!\n", numTimeSyncBytesRead.GetStatus()());
               (void) UnregisterInternalThreadSocket(localTimeSyncUDPSocket, SOCKET_SET_READ);
               localTimeSyncUDPSocket.Reset();
            }
         }
      }

      _hbtState._now = GetRunTime64();
      if (_hbtState._now >= pulseTime)
      {
         _hbtState.Pulse(messagesForOwnerThread);

         MessageRef nextMsgToOwner;
         while(messagesForOwnerThread.RemoveHead(nextMsgToOwner).IsOK()) (void) SendMessageToOwner(nextMsgToOwner);
      }
      if (waitRet.IsOK())
      {
         if (msgFromOwner()) _hbtState.MessageReceivedFromOwner(msgFromOwner);
                        else break;  // NULL msgFromOwner means it is time for this thread to exit!
      }

      Queue<PacketDataIORef> & dios = _hbtState._multicastDataIOs;
      for (uint32 i=0; i<dios.GetNumItems(); i++)
      {
         PacketDataIORef & dio = dios[i];
         if ((dio())&&(IsInternalThreadSocketReady(dio()->GetReadSelectSocket(), SOCKET_SET_READ))) _hbtState.ReceiveMulticastTraffic(*dio());
      }
   }
}

const ZGPeerID & PZGHeartbeatSession :: GetSeniorPeerID() const
{
   if (_mainThreadPeers.IsEmpty()) return GetDefaultObjectForType<ZGPeerID>();

   // Since we sort the list such that full-peers always appear above junior-only peers,
   // we can safely assume that if the first peer isn't a full-peer, then none of them are.
   const ZGPeerID * firstPeer = _mainThreadPeers.GetFirstKey();
   return ((firstPeer)&&(_mainThreadPeers.GetFirstValue()->Head()()->GetPeerType() == PEER_TYPE_FULL_PEER)) ? *firstPeer : GetDefaultObjectForType<ZGPeerID>();
}

uint64 PZGHeartbeatSession :: GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const
{
   return _hbtState.GetEstimatedLatencyToPeer(peerID);
}

};  // end namespace zg_private

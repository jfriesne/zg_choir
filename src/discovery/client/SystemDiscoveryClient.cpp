#include "zg/discovery/client/SystemDiscoveryClient.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/SignalMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"   // for PR_COMMAND_PING
#include "reflector/ReflectServer.h"
#include "system/DetectNetworkConfigChangesSession.h"
#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"
#include "zg/ZGPeerID.h"

namespace zg {

class DiscoveryClientManagerSession;

class DiscoverySession : public AbstractReflectSession
{
public:
   DiscoverySession(const IPAddressAndPort & multicastIAP, DiscoveryClientManagerSession * manager)
      : _multicastIAP(multicastIAP)
      , _manager(manager)
   {
      // empty
   }

   virtual ~DiscoverySession()
   {
      // empty
   }

   virtual ConstSocketRef CreateDefaultSocket()
   {
      _receiveBuffer = GetByteBufferFromPool(4*1024);
      if (_receiveBuffer())
      {
         ConstSocketRef udpSocket = CreateUDPSocket();
         uint16 retPort;
         if ((udpSocket())&&(BindUDPSocket(udpSocket, 0, &retPort).IsOK())&&(SetSocketBlockingEnabled(udpSocket, false).IsOK()))
         {
            if (AddSocketToMulticastGroup(udpSocket, _multicastIAP.GetIPAddress()).IsError()) return ConstSocketRef();
            return udpSocket;
         }
      }
      return ConstSocketRef();
   }

   virtual DataIORef CreateDataIO(const ConstSocketRef & s)
   {
      UDPSocketDataIORef udpIO(new UDPSocketDataIO(s, false));
      (void) udpIO()->SetPacketSendDestination(_multicastIAP);
      return udpIO;
   }

   virtual void MessageReceivedFromSession(AbstractReflectSession & /*from*/, const MessageRef & msg, void * /*userData*/)
   {
      (void) AddOutgoingMessage(msg);
   }

   virtual io_status_t DoInput(AbstractGatewayMessageReceiver &, uint32 maxBytes);

   virtual io_status_t DoOutput(uint32 maxBytes)
   {
      TCHECKPOINT;

      if (GetGateway()() == NULL) return B_BAD_OBJECT;  // abort sessions that couldn't create a socket

      DataIO & udpIO = *GetGateway()()->GetDataIO()();
      Queue<MessageRef> & oq = GetGateway()()->GetOutgoingMessageQueue();
      uint32 ret = 0;
      while((oq.HasItems())&&(ret < maxBytes))
      {
         ByteBufferRef bufRef = oq.Head()()->FlattenToByteBuffer();
         if (bufRef())
         {
            const int32 bytesSent = udpIO.Write(bufRef()->GetBuffer(), bufRef()->GetNumBytes()).GetByteCount();
            if (bytesSent > 0)
            {
               ret += bytesSent;
               LogTime(MUSCLE_LOG_TRACE, "DiscoverySession %p send " INT32_FORMAT_SPEC " bytes of discovery-ping to %s\n", this, bytesSent, _multicastIAP.ToString()());
            }
         }
         (void) oq.RemoveHead();
      }
      return ret;
   }

   virtual void MessageReceivedFromGateway(const muscle::MessageRef&, void*) {/* empty */}

private:
   const IPAddressAndPort _multicastIAP;
   DiscoveryClientManagerSession * _manager;

   ByteBufferRef _receiveBuffer;
};
DECLARE_REFTYPES(DiscoverySession);

// This class will set up new DiscoverySessions as necessary when the network config changes.
class DiscoveryClientManagerSession : public AbstractReflectSession, public INetworkConfigChangesTarget
{
public:
   DiscoveryClientManagerSession(DiscoveryImplementation * imp, const ConstQueryFilterRef & optFilter, uint64 pingInterval)
      : _imp(imp)
      , _pingInterval(pingInterval)
      , _nextPingTime(GetRunTime64())
      , _queryFilter(optFilter)
      , _updateResultSetPending(false)
   {
      // empty
   }

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      return AbstractMessageIOGatewayRef(new SignalMessageIOGateway());
   }

   virtual status_t AttachedToServer()
   {
      MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());

      _pingMsg = GetMessageFromPool(PR_COMMAND_PING);
      MRETURN_OOM_ON_NULL(_pingMsg());
      MRETURN_ON_ERROR(_pingMsg()->CAddArchiveMessage(ZG_DISCOVERY_NAME_FILTER, _queryFilter));

      AddNewDiscoverySessions();
      return B_NO_ERROR;
   }

   virtual void AboutToDetachFromServer()
   {
      EndExistingDiscoverySessions();
      AbstractReflectSession::AboutToDetachFromServer();
   }

   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & /*optInterfaceNames*/)
   {
      LogTime(MUSCLE_LOG_DEBUG, "DiscoveryClientManagerSession:  NetworkInterfacesChanged!  Recreating DiscoverySessions...\n");
      EndExistingDiscoverySessions();
      AddNewDiscoverySessions();
   }

   virtual uint64 GetPulseTime(const PulseArgs & args)
   {
      return _updateResultSetPending ? args.GetCallbackTime() : muscleMin(_nextPingTime, AbstractReflectSession::GetPulseTime(args));
   }

   virtual void Pulse(const PulseArgs & args)
   {
      const uint64 now = args.GetCallbackTime();

      const int64 lateBy = now-args.GetScheduledTime();
      if ((lateBy >= 0)&&(((uint64)lateBy) >= _pingInterval))
      {
         // If our timing has been thrown way off because of App Nap, then we'll just pretend we got an incoming network
         // packet from everybody.  That way we avoid spurious system-is-gone updates due to Apple not waking up our
         // process at the time(s) we asked it to be awoken.
         const uint64 expTime = GetExpirationTime(now);
         for (HashtableIterator<RawDiscoveryKey, RawDiscoveryResult> iter(_rawDiscoveryResults); iter.HasData(); iter++)
            (void) iter.GetValue().Update(iter.GetValue().GetData(), expTime);
      }

      if (now >= _nextPingTime)
      {
         BroadcastToAllSessionsOfType<DiscoverySession>(_pingMsg);

         for (HashtableIterator<RawDiscoveryKey, RawDiscoveryResult> iter(_rawDiscoveryResults); iter.HasData(); iter++)
         {
            if (now >= iter.GetValue().GetExpirationTime())
            {
               (void) _rawDiscoveryResults.Remove(iter.GetKey());
               _updateResultSetPending = true;
            }
         }

         _nextPingTime = now+_pingInterval;
      }

      if (_updateResultSetPending)
      {
         _updateResultSetPending = false;
         UpdateResultSet();
      }
   }

   virtual void ComputerIsAboutToSleep();
   virtual void ComputerJustWokeUp();

   void RawDiscoveryResultReceived(const IPAddressAndPort & localIAP, const IPAddressAndPort & sourceIAP, ZGPeerID peerID, MessageRef & dataMsg)
   {
      if ((_queryFilter())&&(_queryFilter()->Matches(dataMsg, NULL) == false)) return;  // just in case the server didn't filter correctly, we'll also filter here

      RawDiscoveryResult * r = _rawDiscoveryResults.GetOrPut(RawDiscoveryKey(localIAP, sourceIAP, peerID));
      if ((r)&&(r->Update(dataMsg, GetExpirationTime(GetRunTime64())).IsOK())&&(_updateResultSetPending == false))
      {
         _updateResultSetPending = true;
         InvalidatePulseTime();
      }
   }

   virtual void MessageReceivedFromGateway(const MessageRef &, void *);

private:
   /** Given a base time at which a packet was received (e.g. as returned by GetRunTime64(), returns the time in the near future when
     * the source that packet was received from shoudl be expired from the active-sources list, if no further packets are received
     * from that source.
     */
   uint64 GetExpirationTime(uint64 baseTime) const {return (baseTime == MUSCLE_TIME_NEVER) ? baseTime : (baseTime+(3*_pingInterval));}

   void EndExistingDiscoverySessions()
   {
      // First, tell any existing DiscoverySessions to go away
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         DiscoverySession * lds = dynamic_cast<DiscoverySession *>(iter.GetValue()());
         if (lds) lds->EndSession();
      }
   }

   void AddNewDiscoverySessions()
   {
      // Now set up new DiscoverySessions based on our current network config
      Hashtable<IPAddressAndPort, bool> q;
      if (GetDiscoveryMulticastAddresses(q).IsOK())
      {
         for (HashtableIterator<IPAddressAndPort, bool> iter(q); iter.HasData(); iter++)
         {
            // Use a different socket for each IP address, to avoid Mac routing problems
            const IPAddressAndPort & iap = iter.GetKey();
            DiscoverySessionRef ldsRef(new DiscoverySession(iap, this));
            status_t ret;
            if (AddNewSession(ldsRef).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Could not create discovery session for [%s] [%s]\n", iap.ToString()(), ret());
         }
      }
   }

   Hashtable<String, MessageRef> CalculateCookedResults() const
   {
      Hashtable<String, Hashtable<ZGPeerID, MessageRef> > systemNameToPeerIDToInfo;

      // Recompile our results, indexed by system name and peer ID
      for (HashtableIterator<RawDiscoveryKey, RawDiscoveryResult> iter(_rawDiscoveryResults); iter.HasData(); iter++)
      {
         const MessageRef & result = iter.GetValue().GetData();

         const String * systemName;
         ZGPeerID peerID;
         if ((result()->FindFlat(ZG_DISCOVERY_NAME_PEERID, peerID).IsOK())&&(result()->FindString(ZG_DISCOVERY_NAME_SYSTEMNAME, &systemName).IsOK()))
         {
            Hashtable<ZGPeerID, MessageRef> * systemTable = systemNameToPeerIDToInfo.GetOrPut(*systemName);
            if (systemTable) (void) systemTable->Put(peerID, result);
         }
      }

      // Convert results into a set of Messages, one per system
      Hashtable<String, MessageRef> ret;
      for (HashtableIterator<String, Hashtable<ZGPeerID, MessageRef> > iter(systemNameToPeerIDToInfo); iter.HasData(); iter++)
      {
         MessageRef systemMsg = GetMessageFromPool();
         if (systemMsg())
         {
            for (HashtableIterator<ZGPeerID, MessageRef> subIter(iter.GetValue()); subIter.HasData(); subIter++) (void) systemMsg()->AddMessage(ZG_DISCOVERY_NAME_PEERINFO, subIter.GetValue());
            (void) ret.Put(iter.GetKey(), systemMsg);
         }
      }

      return ret;
   }

   void UpdateResultSet();

   class RawDiscoveryKey
   {
   public:
      RawDiscoveryKey() {/* empty */}
      RawDiscoveryKey(const IPAddressAndPort & localIAP, const IPAddressAndPort & sourceIAP, ZGPeerID peerID) : _localIAP(localIAP), _sourceIAP(sourceIAP), _peerID(peerID) {/* empty */}

      bool operator == (const RawDiscoveryKey & rhs) const {return ((_localIAP == rhs._localIAP)&&(_sourceIAP == rhs._sourceIAP)&&(_peerID == rhs._peerID));}
      bool operator != (const RawDiscoveryKey & rhs) const {return !(*this == rhs);}

      bool operator < (const RawDiscoveryKey & rhs) const
      {
         if (_localIAP  < rhs._localIAP) return true;
         if (_localIAP == rhs._localIAP)
         {
            if (_sourceIAP  < rhs._sourceIAP) return true;
            if (_sourceIAP == rhs._sourceIAP) return (_peerID < rhs._peerID);
         }
         return false;
      }

      bool operator > (const RawDiscoveryKey & rhs) const {return ((*this != rhs)&&(!(*this < rhs)));}

      uint32 HashCode() const {return _localIAP.HashCode() + (3*_sourceIAP.HashCode()) + _peerID.HashCode();}

      String ToString() const {return String("RDK:  local=[%1] source=[%2] serial=[%3]").Arg(_localIAP.ToString()).Arg(_sourceIAP.ToString()).Arg(_peerID.ToString());}

   private:
      IPAddressAndPort _localIAP;   // the address-and-port on the local network interface on this machine
      IPAddressAndPort _sourceIAP;  // the address-and-port on the the remote machine
      ZGPeerID _peerID;             // also PeerID, since multiple peers can run on the same machine
   };

   class RawDiscoveryResult
   {
   public:
      RawDiscoveryResult() : _expirationTime(0) {/* empty */}
      RawDiscoveryResult(const MessageRef & data, uint64 expirationTime) : _data(data), _expirationTime(expirationTime) {/* empty */}

      // Returns B_NO_ERROR if anything actually changed, or B_ERROR if nothing changed except our expiration time
      status_t Update(const MessageRef & newData, uint64 newExpirationTime)
      {
         _expirationTime = newExpirationTime;
         if (_data.IsDeeplyEqualTo(newData)) return B_ERROR;  // nothing is different

         _data = newData;
         return B_NO_ERROR;
      }

      const MessageRef & GetData() const {return _data;}
      uint64 GetExpirationTime() const {return _expirationTime;}

   private:
      MessageRef _data;
      uint64 _expirationTime;
   };
   Hashtable<RawDiscoveryKey, RawDiscoveryResult> _rawDiscoveryResults;  // IPs -> PeerInfo
   Hashtable<String, MessageRef> _reportedCookedResults;  // system-name -> SystemInfo

   DiscoveryImplementation * _imp;

   const uint64 _pingInterval;
   MessageRef _pingMsg;
   uint64 _nextPingTime;
   ConstQueryFilterRef _queryFilter;
   bool _updateResultSetPending;
};

io_status_t DiscoverySession :: DoInput(AbstractGatewayMessageReceiver &, uint32 maxBytes)
{
   PacketDataIO * udpIO = dynamic_cast<PacketDataIO *>(GetGateway()()->GetDataIO()());
   if (udpIO == NULL) return B_BAD_OBJECT;  // paranoia

   uint32 ret = 0;
   while(ret < maxBytes)
   {
      IPAddressAndPort sourceLoc;
      const int32 bytesRead = udpIO->ReadFrom(_receiveBuffer()->GetBuffer(), _receiveBuffer()->GetNumBytes(), sourceLoc).GetByteCount();
      if (bytesRead > 0)
      {
         LogTime(MUSCLE_LOG_TRACE, "DiscoverySession %p read " INT32_FORMAT_SPEC " bytes of multicast-reply data from %s\n", this, bytesRead, sourceLoc.ToString()());

         ZGPeerID peerID;
         MessageRef newDataMsg = GetMessageFromPool(*_receiveBuffer());
         if ((newDataMsg())
           &&(newDataMsg()->what == PR_RESULT_PONG)
           &&(newDataMsg()->FindFlat( ZG_DISCOVERY_NAME_PEERID, peerID).IsOK())
           &&(newDataMsg()->AddString(ZG_DISCOVERY_NAME_SOURCE, sourceLoc.ToString()).IsOK()))
              _manager->RawDiscoveryResultReceived(_multicastIAP, sourceLoc, peerID, newDataMsg);

         ret += bytesRead;
      }
      else break;
   }
   return ret;
}

enum {
   DISCOVERY_EVENT_TYPE_UPDATE = 0,
   DISCOVERY_EVENT_TYPE_SLEEP,
   DISCOVERY_EVENT_TYPE_AWAKE,
   NUM_DISCOVERY_EVENT_TYPES
};

class DiscoveryImplementation : private Thread
{
public:
   DiscoveryImplementation(SystemDiscoveryClient & master) : _master(master)
   {
      // empty
   }

   virtual ~DiscoveryImplementation()
   {
      Stop();  // paranoia
   }

   status_t Start()
   {
      Stop();  // paranoia
      return StartInternalThread();
   }

   void Stop()
   {
      (void) ShutdownInternalThread();
      _pendingUpdates.Clear();
   }

   void GrabUpdates(Hashtable<String, MessageRef> & updates)
   {
      DECLARE_MUTEXGUARD(_mutex);
      updates.SwapContents(_pendingUpdates);
   }

   // callable from any thread
   void ScheduleDiscoveryUpdate() {_master.RequestCallbackInDispatchThread(1<<DISCOVERY_EVENT_TYPE_UPDATE);}

protected:
   virtual void InternalThreadEntry()
   {
      ReflectServer server;

      // We need to keep track of when the set of available network interfaces changes
      status_t ret;
      DetectNetworkConfigChangesSession dnccs;
      if (server.AddNewSession(DummyAbstractReflectSessionRef(dnccs)).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "DiscoveryServer:  Couldn't add DetectNetworkChangesSession! [%s]\n", ret());
         return;
      }

      // We need to watch our notification-socket to know when it is time to exit
      DiscoveryClientManagerSession cdms(this, _master._queryFilter, _master._pingInterval);
      if (server.AddNewSession(DummyAbstractReflectSessionRef(cdms), GetInternalThreadWakeupSocket()).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "DiscoveryServer:  Couldn't add DiscoveryClientManagerSession! [%s]\n", ret());
         return;
      }

      (void) server.ServerProcessLoop();
      server.Cleanup();
   }

   void ReportSystemUpdate(const String & systemName, const MessageRef & msg)
   {
      bool sendSignal = false;
      {
         DECLARE_MUTEXGUARD(_mutex);
         sendSignal = (_pendingUpdates.Put(systemName, msg).IsOK());
      }
      if (sendSignal) ScheduleDiscoveryUpdate();
   }

   void ReportSleepNotification(bool isAboutToSleep)
   {
      const int setBitIdx = isAboutToSleep ? DISCOVERY_EVENT_TYPE_SLEEP : DISCOVERY_EVENT_TYPE_AWAKE;
      const int clrBitIdx = isAboutToSleep ? DISCOVERY_EVENT_TYPE_AWAKE : DISCOVERY_EVENT_TYPE_SLEEP;
      _master.RequestCallbackInDispatchThread(1<<setBitIdx, 1<<clrBitIdx);  // the two bits are mutually exclusive
   }

private:
   friend class DiscoveryClientManagerSession;

   // called in internal thread
   status_t HandleMessagesFromOwner()
   {
      MessageRef msg;
      while(WaitForNextMessageFromOwner(msg, 0).IsOK())
      {
         if (msg())
         {
            LogTime(MUSCLE_LOG_TRACE, "DiscoveryClientManagerSession:  Got the following Message from the main thread:\n");
            msg()->PrintToStream();
         }
         else return B_ERROR;  // time for this thread to go away!
      }
      return B_NO_ERROR;
   }

   SystemDiscoveryClient & _master;

   Mutex _mutex;
   Hashtable<String, MessageRef> _pendingUpdates; // access to this must be serialized via _mutex
};

void DiscoveryClientManagerSession :: MessageReceivedFromGateway(const MessageRef &, void *)
{
   if (_imp->HandleMessagesFromOwner().IsError()) EndServer();
}

void DiscoveryClientManagerSession :: ComputerIsAboutToSleep() {_imp->ReportSleepNotification(true);}
void DiscoveryClientManagerSession :: ComputerJustWokeUp()     {_imp->ReportSleepNotification(false);}

void DiscoveryClientManagerSession :: UpdateResultSet()
{
   _rawDiscoveryResults.SortByKey();  // try to maintain a consistent ordering, to avoid churn
   Hashtable<String, MessageRef> newCookedResults = CalculateCookedResults();

   // First, report any systems that have gone missing
   for (HashtableIterator<String, MessageRef> iter(_reportedCookedResults); iter.HasData(); iter++)
   {
      const String & systemName = iter.GetKey();
      if (newCookedResults.ContainsKey(systemName) == false) _imp->ReportSystemUpdate(systemName, MessageRef());
   }

   // Then report any systems that are new or that have changed
   for (HashtableIterator<String, MessageRef> iter(newCookedResults); iter.HasData(); iter++)
   {
      const String & systemName  = iter.GetKey();
      const MessageRef & newInfo = iter.GetValue();
      const MessageRef * oldInfo = _reportedCookedResults.Get(systemName);
      if ((oldInfo == NULL)||(oldInfo->IsDeeplyEqualTo(newInfo) == false)) _imp->ReportSystemUpdate(systemName, newInfo);
   }

   // Finally, adopt the new set
   _reportedCookedResults.SwapContents(newCookedResults);
}

SystemDiscoveryClient :: SystemDiscoveryClient(ICallbackMechanism * mechanism, const String & signaturePattern, const ConstQueryFilterRef & optFilter)
   : ICallbackSubscriber(mechanism)
   , _pingInterval(0)
{
   ConstQueryFilterRef signatureFilterRef(new StringQueryFilter(ZG_DISCOVERY_NAME_SIGNATURE, StringQueryFilter::OP_SIMPLE_WILDCARD_MATCH, signaturePattern));
   if (optFilter()) _queryFilter.SetRef(new AndQueryFilter(signatureFilterRef, optFilter));
               else _queryFilter = signatureFilterRef;

   _imp = new DiscoveryImplementation(*this);
}

SystemDiscoveryClient :: ~SystemDiscoveryClient()
{
   while(_targets.HasItems()) _targets.GetFirstKeyWithDefault()->SetDiscoveryClient(NULL);  // un-register everyone to avoid dangling pointer issues
   delete _imp;
}

status_t SystemDiscoveryClient :: Start(uint64 micros)
{
   const uint64 opi = _pingInterval;

   Stop();

   status_t ret;
   _pingInterval = micros;
   if (_imp->Start().IsOK(ret)) return B_NO_ERROR;

   _pingInterval = opi;  // roll back!
   return ret;
}

void SystemDiscoveryClient :: Stop()
{
   _imp->Stop();
   _pingInterval = 0;
   _knownInfos.Clear();
}

// Called in the main thread
void SystemDiscoveryClient :: MainThreadNotifyAllOfSleepChange(bool isAboutToSleep)
{
   for (HashtableIterator<IDiscoveryNotificationTarget *, bool> iter(_targets); iter.HasData(); iter++)
   {
      IDiscoveryNotificationTarget * t = iter.GetKey();
      if (isAboutToSleep) t->ComputerIsAboutToSleep();
                     else t->ComputerJustWokeUp();
   }
}

// Called in the main thread
void SystemDiscoveryClient :: DispatchCallbacks(uint32 eventTypeBits)
{
   if (eventTypeBits & (1<<DISCOVERY_EVENT_TYPE_UPDATE))
   {
      Hashtable<String, MessageRef> updates;
      _imp->GrabUpdates(updates);

      // Notify all the targets of discovery update
      for (HashtableIterator<IDiscoveryNotificationTarget *, bool> iter(_targets); iter.HasData(); iter++)
      {
         IDiscoveryNotificationTarget * target = iter.GetKey();

         bool & isNewTarget = iter.GetValue();
         if (isNewTarget)
         {
            // fill in the new guy on all known updates (except for the ones we're going to cover below anyway)
            for (HashtableIterator<String, MessageRef> subIter(_knownInfos); subIter.HasData(); subIter++)
            {
               const String & systemName = subIter.GetKey();
               if (updates.ContainsKey(systemName) == false) target->DiscoveryUpdate(systemName, subIter.GetValue());
            }
            isNewTarget = false;
         }

         for (HashtableIterator<String, MessageRef> subIter(updates); subIter.HasData(); subIter++) target->DiscoveryUpdate(subIter.GetKey(), subIter.GetValue());
      }

      // And finally, update our known-updates table
      for (HashtableIterator<String, MessageRef> iter(updates); iter.HasData(); iter++) (void) _knownInfos.PutOrRemove(iter.GetKey(), iter.GetValue());
   }

   if (eventTypeBits & (1<<DISCOVERY_EVENT_TYPE_SLEEP)) MainThreadNotifyAllOfSleepChange(true);
   if (eventTypeBits & (1<<DISCOVERY_EVENT_TYPE_AWAKE)) MainThreadNotifyAllOfSleepChange(false);
}

void IDiscoveryNotificationTarget :: SetDiscoveryClient(SystemDiscoveryClient * discoveryClient)
{
   if (discoveryClient != _discoveryClient)
   {
      if (_discoveryClient) _discoveryClient->UnregisterTarget(this);
      _discoveryClient = discoveryClient;
      if (_discoveryClient) _discoveryClient->RegisterTarget(this);
   }
}

void SystemDiscoveryClient :: RegisterTarget(IDiscoveryNotificationTarget * newTarget)
{
   if (_targets.Put(newTarget, true).IsOK()) _imp->ScheduleDiscoveryUpdate(); // make sure the new guys learns ASAP about whatever we already know
}

void SystemDiscoveryClient :: UnregisterTarget(IDiscoveryNotificationTarget * target)
{
   (void) _targets.Remove(target);
}

};  // end namespace zg

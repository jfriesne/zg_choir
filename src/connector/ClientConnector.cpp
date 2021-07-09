#include "zg/connector/ClientConnector.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for ZG_DISCOVER_NAME_*
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"
#include "zg/messagetree/client/ClientSideNetworkTreeGateway.h"  // for GetPongForLocalSyncPing()
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "iogateway/SignalMessageIOGateway.h"
#include "reflector/AbstractReflectSession.h"
#include "reflector/ReflectServer.h"
#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_BATCH
#include "util/NetworkInterfaceInfo.h"
#include "util/SocketCallbackMechanism.h"
#include "util/SocketMultiplexer.h"
#include "system/Thread.h"

namespace zg {

enum {
   CCI_COMMAND_PEERINFO = 1667459427, // 'ccic'
   CCI_COMMAND_MESSAGE,
};
static const String CCI_NAME_PAYLOAD = "pay";

class TCPConnectorSession;

static uint64 GetPreferredTimeSyncSendIntervalMicros(const IPAddressAndPort & timeSyncDest)
{
#ifdef __APPLE__
   // If the specified destination is on a WiFi adapter, let's ping faster than 5Hz to
   // avoid Apple's power-down-the-WiFi-after-200mS-of-inactivity timeout, which would otherwise
   // introduce a lot of unnecessary jitter into our measured round-trip-times due to the
   // WiFi hardware taking a random amount of time (up to several dozen milliseconds) to
   // wake up again.
   const uint32 iidx = timeSyncDest.GetIPAddress().GetInterfaceIndex();
   if (iidx > 0)
   {
      Queue<NetworkInterfaceInfo> niiq;
      if (GetNetworkInterfaceInfos(niiq).IsOK())
      {
         for (uint32 i=0; i<niiq.GetNumItems(); i++)
         {
            const NetworkInterfaceInfo & nii = niiq[i];
            if ((nii.GetLocalAddress().GetInterfaceIndex() == iidx)&&(nii.GetHardwareType() == NETWORK_INTERFACE_HARDWARE_TYPE_WIFI)) return MillisToMicros(150);
         }
      }
   }
#endif
   return SecondsToMicros(1);  // for normal cases, one time-sync per second should be plenty
}

// This session will periodically UDP-ping the heartbeat-thread of the ZG peer we are connected to,
// so that we can approximate the peer's network-time clock on this client
class UDPTimeSyncSession : public AbstractReflectSession
{
public:
   UDPTimeSyncSession(TCPConnectorSession * master, const IPAddressAndPort & timeSyncDest)
   : _master(master)
   , _timeSyncDest(timeSyncDest)
   , _nextUDPSendTime(0)
   , _sendIntervalMicros(GetPreferredTimeSyncSendIntervalMicros(timeSyncDest))
   {
      // empty
   }

   virtual ConstSocketRef CreateDefaultSocket()
   {
      ConstSocketRef udpSocket = CreateUDPSocket();
      if (udpSocket() == NULL)
      {
         LogTime(MUSCLE_LOG_ERROR, "UDPTimeSyncSession:  CreateUDPSocket() failed!\n");
         return ConstSocketRef();
      }

      const status_t ret = BindUDPSocket(udpSocket, 0);
      if (ret.IsError())
      {
         LogTime(MUSCLE_LOG_ERROR, "UDPTimeSyncSession:  BindUDPSocket() failed! [%s]\n", ret());
         return ConstSocketRef();
      }

      return udpSocket;
   }

   virtual DataIORef CreateDataIO(const ConstSocketRef & socket)
   {
      UDPSocketDataIORef dio(newnothrow UDPSocketDataIO(socket, false));
      if (dio() == NULL) {MWARN_OUT_OF_MEMORY; return DataIORef();}

      (void) dio()->SetPacketSendDestination(_timeSyncDest);
      return dio;
   }

   virtual uint64 GetPulseTime(const PulseArgs & args)
   {
      return muscleMin(AbstractReflectSession::GetPulseTime(args), _nextUDPSendTime);
   }

   virtual void Pulse(const PulseArgs & args)
   {
      AbstractReflectSession::Pulse(args);

      if (args.GetCallbackTime() >= _nextUDPSendTime)
      {
         MessageRef pingMsg = GetMessageFromPool(0);
         if ((pingMsg())&&(pingMsg()->AddInt64("ctm", GetRunTime64()).IsOK()))
         {
            (void) AddOutgoingMessage(pingMsg);
            (void) DoOutput(MUSCLE_NO_LIMIT);  // since this message is time-sensitive, let's try to send it right away rather than waiting for the event-loop to get to it
         }
         _nextUDPSendTime = args.GetCallbackTime() + _sendIntervalMicros;
      }
   }

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *);

   virtual void EndSession()
   {
      _master = NULL;  // avoid dangling-pointer after our TCPConnectorSession goes away
      AbstractReflectSession::EndSession();
   }

   void SchedulePing(uint64 when)
   {
      _nextUDPSendTime = when;
      InvalidatePulseTime();
   }

private:
   TCPConnectorSession * _master;
   const IPAddressAndPort _timeSyncDest;
   uint64 _nextUDPSendTime;
   const uint64 _sendIntervalMicros;
};
DECLARE_REFTYPES(UDPTimeSyncSession);

static const String _inactivityPingIDField("tcp_cs");

// This class handles TCP I/O to/from the ZG server
class TCPConnectorSession : public AbstractReflectSession
{
public:
   TCPConnectorSession(ClientConnectorImplementation * master, const IPAddressAndPort & timeSyncDest, const MessageRef & peerInfo, uint64 inactivityPingTimeMicroseconds)
   : _master(master)
   , _timeSyncDest(timeSyncDest)
   , _peerInfo(peerInfo)
   , _inactivityPingTimeMicroseconds(inactivityPingTimeMicroseconds)
   , _nextInactivityPingTime(MUSCLE_TIME_NEVER)
   , _lastDataReadTime(0)
   , _lastInactivityPingTime(0)
   , _inactivityPingMsg(PR_COMMAND_PING)
   , _inactivityPingResponsePending(false)
   {
      // empty
   }

   virtual status_t AttachedToServer()
   {
      MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());
      MRETURN_ON_ERROR(_inactivityPingMsg.AddBool(_inactivityPingIDField, true));

      if (_timeSyncDest.IsValid())
      {
         _timeSyncSession.SetRef(newnothrow UDPTimeSyncSession(this, _timeSyncDest));
         MRETURN_OOM_ON_NULL(_timeSyncSession());
         MRETURN_ON_ERROR(AddNewSession(_timeSyncSession));
      }

      return B_NO_ERROR;
   }

   virtual int32 DoInput(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
   {
      const int32 ret = AbstractReflectSession::DoInput(receiver, maxBytes);
      if (ret > 0) RecordThatDataWasRead();
      return ret;
   }

   virtual uint64 GetPulseTime(const PulseArgs & args) {return muscleMin(_nextInactivityPingTime, AbstractReflectSession::GetPulseTime(args));}

   virtual void Pulse(const PulseArgs & args)
   {
      AbstractReflectSession::Pulse(args);
      if (args.GetCallbackTime() >= _nextInactivityPingTime)
      {
         _lastInactivityPingTime = args.GetCallbackTime();
         if ((_inactivityPingResponsePending == false)&&(AddOutgoingMessage(DummyMessageRef(_inactivityPingMsg)).IsOK())) _inactivityPingResponsePending = true;

         if ((_inactivityPingTimeMicroseconds > 0)&&(_timeSyncSession()))
         {
            const uint32 overdueCount = (args.GetScheduledTime()-_lastDataReadTime)/_inactivityPingTimeMicroseconds;
                 if (overdueCount >= 5) (void) DisconnectSession();                                // connectivity lost, force a failover
            else if (overdueCount >= 2) _timeSyncSession()->SchedulePing(args.GetCallbackTime());  // to check up on the server's health ASAP
         }

         UpdateNextInactivityPingTime();
      }
   }

   virtual void AboutToDetachFromServer()
   {
      if (_timeSyncSession()) _timeSyncSession()->EndSession();  // avoid any chance of it holding a dangling-pointer to us
      AbstractReflectSession::AboutToDetachFromServer();
   }

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *);

   virtual bool ClientConnectionClosed()
   {
      const bool ret = AbstractReflectSession::ClientConnectionClosed();
      if (ret)
      {
         if (_timeSyncSession()) _timeSyncSession()->EndSession();  // avoid any chance of it calling methods on us after this
         TimeSyncReceived(0, MUSCLE_TIME_NEVER, GetRunTime64());    // turn off the client-side clock, as we can't sync it with the server's clock if we aren't in touch with the server
         _nextInactivityPingTime = MUSCLE_TIME_NEVER;
         EndServer();
      }
      return ret;
   }

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      MessageIOGatewayRef ret(newnothrow MessageIOGateway());
      if (ret()) ret()->SetAboutToFlattenMessageCallback(WatchForLocalPingMessagesCallbackFunc, this);
            else MWARN_OUT_OF_MEMORY;
      return ret;
   }

   virtual void AsyncConnectCompleted();

   void TimeSyncReceived(uint64 roundTripTime, uint64 serverNetworkTime, uint64 localReceiveTime);

private:
   static status_t WatchForLocalPingMessagesCallbackFunc(const MessageRef & msgRef, void * ud) {return ((TCPConnectorSession*)ud)->WatchForLocalPingMessagesCallback(msgRef);}
   status_t WatchForLocalPingMessagesCallback(const MessageRef & msgRef)
   {
      if (msgRef()->what == PR_COMMAND_BATCH)
      {
         Queue<MessageRef> pongMessages;
         StripBatchSyncPings(*msgRef(), pongMessages);
         for (uint32 i=0; i<pongMessages.GetNumItems(); i++) CallMessageReceivedFromGateway(pongMessages[i], NULL);
      }
      else
      {
         MessageRef pongMsg = GetPongForLocalSyncPing(*msgRef());
         if (pongMsg())
         {
            CallMessageReceivedFromGateway(pongMsg, NULL);
            return B_ERROR;  // don't send the sync-ping out over TCP
         }
      }
      return B_NO_ERROR;
   }

   void StripBatchSyncPings(Message & batchMsg, Queue<MessageRef> & retPongMessages) const
   {
      MessageRef subMsg;
      for (int32 i=0; batchMsg.FindMessage(PR_NAME_KEYS, i, subMsg).IsOK(); i++)
      {
         MessageRef pongMsg = GetPongForLocalSyncPing(*subMsg());
         if (pongMsg())
         {
            (void) batchMsg.RemoveData(PR_NAME_KEYS, i);
            i--;
            retPongMessages.AddTail(pongMsg);
         }
         else if (subMsg()->what == PR_COMMAND_BATCH) StripBatchSyncPings(*subMsg(), retPongMessages);
      }
   }

   void RecordThatDataWasRead()
   {
      _lastDataReadTime = GetRunTime64();
      UpdateNextInactivityPingTime();
   }

   void UpdateNextInactivityPingTime()
   {
      _nextInactivityPingTime = (_inactivityPingTimeMicroseconds==MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (muscleMax(_lastDataReadTime,_lastInactivityPingTime)+_inactivityPingTimeMicroseconds);
      InvalidatePulseTime();
   }

   ClientConnectorImplementation * _master;
   IPAddressAndPort _timeSyncDest;
   UDPTimeSyncSessionRef _timeSyncSession;
   MessageRef _peerInfo;

   const uint64 _inactivityPingTimeMicroseconds;
   uint64 _nextInactivityPingTime;
   uint64 _lastDataReadTime;
   uint64 _lastInactivityPingTime;
   Message _inactivityPingMsg;
   bool _inactivityPingResponsePending;  // true iff we've send a keepalive PR_COMMAND_PING over TCP and are currently waiting for the corresponding PR_RESULT_PONG to come back
};

void UDPTimeSyncSession :: MessageReceivedFromGateway(const MessageRef & msg, void *)
{
   if (_master)
   {
      const uint64 localReceiveTime = GetRunTime64();
      _master->TimeSyncReceived(localReceiveTime-msg()->GetInt64("ctm"), msg()->GetInt64("stm"), localReceiveTime);
   }
}

// This class handles signals from the owner thread, while we're in the connection-stage
class MonitorOwnerThreadSession : public AbstractReflectSession
{
public:
   MonitorOwnerThreadSession(ClientConnectorImplementation * master, const ConstSocketRef & notifySock)
      : _master(master)
      , _notifySock(notifySock)
   {
      // empty
   }

   virtual ConstSocketRef CreateDefaultSocket()
   {
      return _notifySock;
   }

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      return AbstractMessageIOGatewayRef(new SignalMessageIOGateway);
   }

   virtual void MessageReceivedFromGateway(const MessageRef &, void *);

   virtual bool ClientConnectionClosed()
   {
      const bool ret = AbstractReflectSession::ClientConnectionClosed();
      if (ret) EndServer();
      return ret;
   }

private:
   ClientConnectorImplementation * _master;
   ConstSocketRef _notifySock;
};

static bool AreDiscoveryCriteriaEqual(const ConstQueryFilterRef & c1, const ConstQueryFilterRef & c2)
{
   if ((c1() != NULL) != (c2() != NULL)) return false;
   if (c1() == NULL)                     return false;

   // At this point we are guaranteed that both references are non-NULL.
   // Since I don't have any kind of IsEqualTo() methods defined for the QueryFilter
   // class hierarchy, I'm going to cheat by dumping both filters into Message objects,
   // and then seeing if the created Message objects are equal.
   Message m1, m2;
   return ((c1()->SaveToArchive(m1).IsOK())&&(c2()->SaveToArchive(m2).IsOK())&&(m1 == m2));
}

class ClientConnectorImplementation : private Thread, private IDiscoveryNotificationTarget
{
public:
   ClientConnectorImplementation(ClientConnector * master)
      : IDiscoveryNotificationTarget(NULL)
      , _master(master)
      , _reconnectTimeMicroseconds(0)
      , _inactivityPingTimeMicroseconds(0)
      , _isActive(false)
      , _tcpSession(NULL)
      , _keepGoing(true)
   {
      // empty
   }

   bool IsActive() const {return _isActive;}

   status_t Start(const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalDiscoveryCriteria, uint64 reconnectTimeMicroseconds, uint64 inactivityPingTimeMicroseconds)
   {
      // If we're already in the desired state, then there's no point in tearing everything down only to set it up again, so instead just smile and nod
      if ((_isActive)&&(signaturePattern == _signaturePattern)&&(systemNamePattern == _systemNamePattern)&&(AreDiscoveryCriteriaEqual(optAdditionalDiscoveryCriteria,_optAdditionalDiscoveryCriteria))&&(_reconnectTimeMicroseconds == reconnectTimeMicroseconds)&&(_inactivityPingTimeMicroseconds == inactivityPingTimeMicroseconds)) return B_NO_ERROR;
      else
      {
         Stop();

         // Set these before starting the internal thread, just to avoid potential race conditions should it want to access them
         _signaturePattern               = signaturePattern;
         _systemNamePattern              = systemNamePattern;
         _optAdditionalDiscoveryCriteria = optAdditionalDiscoveryCriteria;
         _reconnectTimeMicroseconds      = reconnectTimeMicroseconds;
         _inactivityPingTimeMicroseconds = inactivityPingTimeMicroseconds;
         _isActive                       = true;

         // This is the filter the DiscoveryModule will actually use (it's a superset of the _optAdditionalDiscoveryCriteria because it also specifies what system name(s) we will accept)
         _discoFilterRef.SetRef(newnothrow StringQueryFilter(ZG_DISCOVERY_NAME_SYSTEMNAME, StringQueryFilter::OP_SIMPLE_WILDCARD_MATCH, _systemNamePattern));
         if ((_discoFilterRef())&&(_optAdditionalDiscoveryCriteria())) _discoFilterRef.SetRef(newnothrow AndQueryFilter(_discoFilterRef, _optAdditionalDiscoveryCriteria));

         const status_t ret = _discoFilterRef() ? StartInternalThread() : B_OUT_OF_MEMORY;
         if (ret.IsError()) Stop();  // roll back the state we set above
         return ret;
      }
   }

   void Stop()
   {
      ShutdownInternalThread();

      // Reset to our default state
      _signaturePattern.Clear();
      _systemNamePattern.Clear();
      _optAdditionalDiscoveryCriteria.Reset();
      _reconnectTimeMicroseconds = 0;
      _inactivityPingTimeMicroseconds = 0;
      _discoFilterRef.Reset();
      _isActive = false;
   }

   status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return SendMessageToInternalThread(msg);}

   const String & GetSignaturePattern()  const {return _signaturePattern;}
   const String & GetSystemNamePattern() const {return _systemNamePattern;}
   const ConstQueryFilterRef & GetAdditionalDiscoveryCriteria() const {return _optAdditionalDiscoveryCriteria;}
   uint64 GetAutoReconnectTimeMicroseconds()  const {return _reconnectTimeMicroseconds;}
   uint64 GetInactivityPingTimeMicroseconds() const {return _inactivityPingTimeMicroseconds;}

private:
   friend class TCPConnectorSession;
   friend class MonitorOwnerThreadSession;

   // Called from within the InternalThread!
   void TimeSyncReceived(uint64 roundTripTime, uint64 serverNetworkTime, uint64 localReceiveTime)
   {
      if (_master) _master->TimeSyncReceived(roundTripTime, serverNetworkTime, localReceiveTime);
   }

   virtual void InternalThreadEntry()
   {
      _keepGoing = true;

      while(_keepGoing)
      {
         // First, find a server to connect to
         if (_keepGoing)
         {
            const status_t ret = DoDiscoveryStage();
            if ((ret.IsError())&&(ret != B_IO_ERROR)) LogTime(MUSCLE_LOG_ERROR, "ClientConnectorImplementation:  Discovery stage encountered an error!  [%s]\n", ret());
         }

         // Now that discovery stage has finished, we can move to our TCP-connection stage
         if ((_keepGoing)&&(_discoveries()))
         {
            const status_t ret = DoConnectionStage();
            if ((ret.IsError())&&(ret != B_IO_ERROR)) LogTime(MUSCLE_LOG_ERROR, "ClientConnectorImplementation:  Connection stage encountered an error!  [%s]\n", ret());
            SetConnectionPeerInfo(MessageRef());  // tell owner thread we're disconnected
         }

         // If we got here, then our TCP connection failed.  Wait the specified delay time before going back to discovery-mode
         if (_keepGoing)
         {
            const status_t ret = DoDelayStage();
            if ((ret.IsError())&&(ret != B_IO_ERROR)) LogTime(MUSCLE_LOG_ERROR, "ClientConnectorImplementation:  Delay stage encountered an error!  [%s]\n", ret());
         }
      }
   }

   status_t DoDiscoveryStage()
   {
      _discoveries.Reset();  // paranoia

      SocketCallbackMechanism threadMechanism;  // so we can get callbacks in this thread from our SystemDiscoveryClient

      SystemDiscoveryClient discoClient(&threadMechanism, _signaturePattern, _discoFilterRef);
      SetDiscoveryClient(&discoClient);

      MRETURN_ON_ERROR(discoClient.Start());

      // Wait until we either discovered something or the calling thread wants us to go away
      SocketMultiplexer sm;
      while(_discoveries() == NULL)
      {
         MRETURN_ON_ERROR(sm.RegisterSocketForReadReady(threadMechanism.GetDispatchThreadNotifierSocket().GetFileDescriptor()));
         MRETURN_ON_ERROR(sm.RegisterSocketForReadReady(GetInternalThreadWakeupSocket().GetFileDescriptor()));
         if (sm.WaitForEvents() < 0) return B_ERROR("ClientConnectorImplementation:  WaitForEvents() failed");

         if (sm.IsSocketReadyForRead(threadMechanism.GetDispatchThreadNotifierSocket().GetFileDescriptor())) threadMechanism.DispatchCallbacks();
         if (sm.IsSocketReadyForRead(GetInternalThreadWakeupSocket().GetFileDescriptor())) MRETURN_ON_ERROR(HandleIncomingMessagesFromOwnerThread());
      }

      return B_NO_ERROR;
   }

   status_t DoConnectionStage()
   {
      if (_discoveries() == NULL) return B_NO_ERROR;
      MessageRef discoveries = _discoveries;  // just so we can return any time without having to unset this first
      _discoveries.Reset();

      // Find an amenable server to connect to
      MessageRef peerInfo;
      IPAddressAndPort iap;
      for (int32 i=0; discoveries()->FindMessage(ZG_DISCOVERY_NAME_PEERINFO, i, peerInfo).IsOK(); i++)
      {
         uint16 port = 0;  // set to zero just to avoid a compiler warning
         const String * ip = peerInfo()->GetStringPointer(ZG_DISCOVERY_NAME_SOURCE);
         if ((ip)&&(_master->ParseTCPPortFromMessage(*peerInfo(), port).IsOK()))
         {
            iap.SetFromString(*ip, 0, false);
            iap.SetPort(port);   // we want to connect to the server's TCP-accepting port, NOT the port it sent us a UDP packet from!
            break;
         }
      }
      if (iap.IsValid() == false) return B_BAD_DATA;

      LogTime(MUSCLE_LOG_DEBUG, "ClientConnector %p connecting to [%s]...\n", this, iap.ToString()());

      IPAddressAndPort timeSyncDest;
      {
         const uint16 timeSyncUDPPort = peerInfo()->GetInt16(ZG_DISCOVERY_NAME_TIMESYNCPORT);
         if (timeSyncUDPPort > 0) timeSyncDest = IPAddressAndPort(iap.GetIPAddress(), timeSyncUDPPort);
      }

      TCPConnectorSession tcs(this, timeSyncDest, peerInfo, _inactivityPingTimeMicroseconds);   // handle TCP I/O to/from our server
      MonitorOwnerThreadSession mots(this, GetInternalThreadWakeupSocket());  // handle Messages and shutdown-requests from our owner-thread

      status_t ret;
      ReflectServer eventLoop;
      if ((eventLoop.AddNewSession(       DummyAbstractReflectSessionRef(mots)).IsOK(ret))
       && (eventLoop.AddNewConnectSession(DummyAbstractReflectSessionRef(tcs), iap.GetIPAddress(), iap.GetPort()).IsOK(ret)))
      {
         _tcpSession = &tcs;
            status_t ret;
            if (eventLoop.ServerProcessLoop().IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "ClientConnector:  ServerProcessLoop() returned [%s]\n", ret());
         _tcpSession = NULL;
      }

      eventLoop.Cleanup();

      return ret;
   }

   status_t DoDelayStage()
   {
      if (_reconnectTimeMicroseconds == 0) return B_NO_ERROR;  // nothing to do!

      SocketMultiplexer sm;
      const uint64 waitUntil = (_reconnectTimeMicroseconds == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+_reconnectTimeMicroseconds);
      while(GetRunTime64() < waitUntil)
      {
         status_t ret;
         if (sm.RegisterSocketForReadReady(GetInternalThreadWakeupSocket().GetFileDescriptor()).IsError(ret)) return ret;
         if (sm.WaitForEvents(waitUntil) < 0) return B_ERROR("ClientConnectorImplementation:  WaitForEvents() failed while in delay state");
         if ((sm.IsSocketReadyForRead(GetInternalThreadWakeupSocket().GetFileDescriptor()))&&(HandleIncomingMessagesFromOwnerThread().IsError(ret))) return ret;
      }
      return B_NO_ERROR;
   }

   virtual void DiscoveryUpdate(const String & /*systemName*/, const MessageRef & optSystemInfo)
   {
      if (optSystemInfo()) _discoveries = optSystemInfo;
   }

   status_t HandleIncomingMessagesFromOwnerThread()
   {
      MessageRef msgRef;
      while(1)
      {
         const int32 numLeft = WaitForNextMessageFromOwner(msgRef, 0);
         if (numLeft  < 0) return B_IO_ERROR;  // it's okay, it just means the owner thread wants us to go away now
         if (numLeft >= 0) MRETURN_ON_ERROR(MessageReceivedFromOwner(msgRef, numLeft));
         if (numLeft == 0) return B_NO_ERROR;
      }
   }

   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 /*numLeft*/)
   {
      if (msgRef() == NULL)
      {
         _keepGoing = false;
         return B_ERROR("Owner thread requested exit.");
      }
      return _tcpSession ? _tcpSession->AddOutgoingMessage(msgRef) : B_NO_ERROR;
   }

   virtual void MessageReceivedFromTCPConnection(const MessageRef & msgRef)
   {
      MessageRef wrapper = GetMessageFromPool(CCI_COMMAND_MESSAGE);
      if ((wrapper())&&(wrapper()->AddMessage(CCI_NAME_PAYLOAD, msgRef).IsOK())) _master->MessageReceivedFromIOThread(wrapper);
   }

   void SetConnectionPeerInfo(const MessageRef & optPeerInfo)
   {
      MessageRef wrapper = GetMessageFromPool(CCI_COMMAND_PEERINFO);
      if ((wrapper())&&(wrapper()->CAddMessage(CCI_NAME_PAYLOAD, optPeerInfo).IsOK())) _master->MessageReceivedFromIOThread(wrapper);
   }

   ClientConnector * _master;

   String _signaturePattern;
   String _systemNamePattern;
   ConstQueryFilterRef _optAdditionalDiscoveryCriteria;  // additional restrictions that the calling code specified (if any)
   uint64 _reconnectTimeMicroseconds;
   uint64 _inactivityPingTimeMicroseconds;
   bool _isActive;
   ConstQueryFilterRef _discoFilterRef;  // cached superset of _optAdditionalDiscoveryCriteria

   // members below this point should be accessed only by the internal thread
   AbstractReflectSession * _tcpSession;
   MessageRef _discoveries;
   bool _keepGoing;
};

void TCPConnectorSession :: TimeSyncReceived(uint64 roundTripTime, uint64 serverNetworkTime, uint64 localReceiveTime)
{
   RecordThatDataWasRead();  // so that we won't force-disconnect just because the server's main thread is busy executing some extended operation

   if (_master) _master->TimeSyncReceived(roundTripTime, serverNetworkTime, localReceiveTime);
}

void TCPConnectorSession :: AsyncConnectCompleted()
{
   AbstractReflectSession::AsyncConnectCompleted();
   _inactivityPingResponsePending = false;  // semi-paranoia
   RecordThatDataWasRead();  // start the inactivity-ping-timeout timer now
   _master->SetConnectionPeerInfo(_peerInfo);
}

void TCPConnectorSession :: MessageReceivedFromGateway(const MessageRef & msg, void *)
{
   if ((msg())&&(msg()->what == PR_RESULT_PONG)&&(msg()->GetBool(_inactivityPingIDField)))
   {
      _inactivityPingResponsePending = false;  // We don't need to do anything other than eat the Message; our DoInput() method was already called and that's all we really wanted
   }
   else _master->MessageReceivedFromTCPConnection(msg);
}

void MonitorOwnerThreadSession :: MessageReceivedFromGateway(const MessageRef &, void *)
{
   if (_master->HandleIncomingMessagesFromOwnerThread().IsError()) EndServer();
}

ClientConnector :: ClientConnector(ICallbackMechanism * mechanism)
   : ICallbackSubscriber(mechanism)
   , _timeAverager(20)
   , _mainThreadToNetworkTimeOffset(INVALID_TIME_OFFSET)
   , _mainThreadLastTimeSyncPongTime(INVALID_TIME_OFFSET)
{
   _imp = new ClientConnectorImplementation(this);
}

ClientConnector :: ~ClientConnector()
{
   MASSERT(IsActive() == false, "~ClientConnector:  You must call Stop() before deleting the ClientConnector object");
   delete _imp;
}

void ClientConnector :: TimeSyncReceived(uint64 roundTripTime, uint64 serverNetworkTime, uint64 localReceiveTime)
{
   if (serverNetworkTime == MUSCLE_TIME_NEVER)
   {
      _timeAverager.Clear();
      _mainThreadToNetworkTimeOffset  = INVALID_TIME_OFFSET;
      _mainThreadLastTimeSyncPongTime = MUSCLE_TIME_NEVER;
   }
   else if (_timeAverager.AddMeasurement(roundTripTime, localReceiveTime).IsOK())
   {
      _mainThreadToNetworkTimeOffset  = serverNetworkTime-(localReceiveTime-(_timeAverager.GetAverageValueIgnoringOutliers()/2));
      _mainThreadLastTimeSyncPongTime = localReceiveTime;
//printf("Added measurement %llu offset is now %lli averager=%llu\n", roundTripTime, (int64) _mainThreadToNetworkTimeOffset, _timeAverager.GetAverageValueIgnoringOutliers());
   }
}

status_t ClientConnector :: ParseTCPPortFromMessage(const Message & msg, uint16 & retPort) const {return msg.FindInt16("port", retPort);}
status_t ClientConnector :: Start(const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalDiscoveryCriteria, uint64 reconnectTimeMicroseconds, uint64 inactivityPingTimeMicroseconds) {return _imp->Start(signaturePattern, systemNamePattern, optAdditionalDiscoveryCriteria, reconnectTimeMicroseconds, inactivityPingTimeMicroseconds);}
const String & ClientConnector :: GetSignaturePattern()  const {return _imp->GetSignaturePattern();}
const String & ClientConnector :: GetSystemNamePattern() const {return _imp->GetSystemNamePattern();}
const ConstQueryFilterRef & ClientConnector :: GetAdditionalDiscoveryCriteria() const {return _imp->GetAdditionalDiscoveryCriteria();}
uint64 ClientConnector :: GetAutoReconnectTimeMicroseconds() const {return _imp->GetAutoReconnectTimeMicroseconds();}
uint64 ClientConnector :: GetInactivityPingTimeMicroseconds() const {return _imp->GetInactivityPingTimeMicroseconds();}

void ClientConnector :: Stop() {_imp->Stop();}
bool ClientConnector :: IsActive() const {return _imp->IsActive();}

void ClientConnector :: DispatchCallbacks(uint32 /*eventTypeBits*/)
{
   // critical section
   {
      MutexGuard mg(_replyQueueMutex);
      _scratchQueue.SwapContents(_replyQueue);
   }

   MessageRef next;
   while(_scratchQueue.RemoveHead(next).IsOK())
   {
      switch(next()->what)
      {
         case CCI_COMMAND_PEERINFO:
            _connectedPeerInfo = next()->GetMessage(CCI_NAME_PAYLOAD);
            ConnectionStatusUpdated(_connectedPeerInfo);
         break;

         case CCI_COMMAND_MESSAGE:
         {
            MessageRef subMsg;
            if (next()->FindMessage(CCI_NAME_PAYLOAD, subMsg).IsOK()) MessageReceivedFromNetwork(subMsg);
         }
         break;

         default:
            LogTime(MUSCLE_LOG_CRITICALERROR, "ClientConnector %p:  DispatchCallbacks:  Unknown Message type " UINT32_FORMAT_SPEC "\n", next()->what);
         break;
      }
   }
}

status_t ClientConnector :: SendOutgoingMessageToNetwork(const MessageRef & msg) {return _imp->SendOutgoingMessageToNetwork(msg);}

// This method is called from within the I/O thread, so we have to be careful with it!
void ClientConnector :: MessageReceivedFromIOThread(const MessageRef & msg)
{
   MutexGuard mg(_replyQueueMutex);
   if (_replyQueue.IsEmpty()) RequestCallbackInDispatchThread();
   (void) _replyQueue.AddTail(msg);
}

};  // end namespace zg

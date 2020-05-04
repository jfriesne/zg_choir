#include "zg/callback/SocketCallbackMechanism.h"
#include "zg/connector/ClientConnector.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for ZG_DISCOVER_NAME_*
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"
#include "util/SocketMultiplexer.h"
#include "system/Thread.h"

namespace zg {

class ClientConnectorImplementation : private Thread, private IDiscoveryNotificationTarget
{
public:
   ClientConnectorImplementation(ClientConnector * master, const String & signaturePattern, const ConstQueryFilterRef & qfRef)
      : IDiscoveryNotificationTarget(NULL)
      , _master(master)
      , _signaturePattern(signaturePattern)
      , _queryFilter(qfRef)
      , _isActive(false)
      , _isConnected(false)
      , _reconnectTimeMicroseconds(0)
   {
      // empty
   }

   bool IsActive() const {return _isActive;}

   status_t Start(uint64 reconnectTimeMicroseconds)
   {
      Stop();
      _reconnectTimeMicroseconds = reconnectTimeMicroseconds;

      _isActive = true;

      const status_t ret = StartInternalThread();
      if (ret.IsError()) _isActive = false;
      return ret;
   }

   void Stop()
   {
      ShutdownInternalThread();
      _isActive = _isConnected = false;
   }

   status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return SendMessageToInternalThread(msg);}

private:
   virtual void InternalThreadEntry()
   {
      while(1)
      {
         status_t ret = DoDiscoveryStage();  // find someone to connect to
         if (ret.IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "ClientConnectorImplementation:  Discovery stage encountered an error!  [%s]\n", ret());
            break;
         }

         // Now that discovery stage has finished, we can move to our TCP-connection stage
         {
            ret = DoConnectionStage();
            if (ret.IsError())
            {
               LogTime(MUSCLE_LOG_ERROR, "ClientConnectorImplementation:  Connection stage encountered an error!  [%s]\n", ret());
               break;
            }
         }

         // If we got here, then our TCP connection failed.  Wait the specified delay time before going back to discovery-mode
         {
            ret = DoDelayStage();
            if (ret.IsError())
            {
               LogTime(MUSCLE_LOG_ERROR, "ClientConnectorImplementation:  Delay stage encountered an error!  [%s]\n", ret());
               break;
            }
         }
      }
   }

   status_t DoDiscoveryStage()
   {
printf("DoDiscovery!\n");
      _discoveries.Reset();  // paranoia

      SocketCallbackMechanism threadMechanism;  // so we can get callbacks in this thread from our SystemDiscoveryClient
      SystemDiscoveryClient discoClient(&threadMechanism, _signaturePattern, _queryFilter);
      SetDiscoveryClient(&discoClient);

      status_t ret;
      if (discoClient.Start().IsError(ret)) return ret;

      // Wait until we either discovered something or the calling thread wants us to go away
      SocketMultiplexer sm;
      while(_discoveries() == NULL)
      {
         if (sm.RegisterSocketForReadReady(threadMechanism.GetDispatchThreadNotifierSocket().GetFileDescriptor()).IsError(ret)) return ret;
         if (sm.RegisterSocketForReadReady(GetInternalThreadWakeupSocket().GetFileDescriptor()).IsError(ret)) return ret;
         if (sm.WaitForEvents() < 0) return B_ERROR("ClientConnectorImplementation:  WaitForEvents() failed");

         if (sm.IsSocketReadyForRead(threadMechanism.GetDispatchThreadNotifierSocket().GetFileDescriptor())) threadMechanism.DispatchCallbacks();
         if ((sm.IsSocketReadyForRead(GetInternalThreadWakeupSocket().GetFileDescriptor()))&&(HandleIncomingMessagesFromOwnerThread().IsError(ret))) return ret;
      }

      return ret;
   }

   status_t DoConnectionStage()
   {
      if (_discoveries() == NULL) return B_NO_ERROR;

printf("CONNECT TO:\n");
_discoveries()->PrintToStream();
_discoveries.Reset();
// _tcpSession = xXXXXx;
// _tcpSession = NULL;

      _isConnected = false;
      return B_NO_ERROR;
   }

   status_t DoDelayStage()
   {
      if (_reconnectTimeMicroseconds == 0) return B_NO_ERROR;  // nothing to do!

      SocketMultiplexer sm;
      const uint64 waitUntil = GetRunTime64()+_reconnectTimeMicroseconds;
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
         status_t ret;
         const int32 numLeft = WaitForNextMessageFromOwner(msgRef, 0);
         if (numLeft < 0) return B_ERROR("ClientConnectorImplementation:  Error retrieving message from owning thread");
         if ((numLeft >= 0)&&(MessageReceivedFromOwner(msgRef, numLeft).IsError(ret))) return ret;
         if (numLeft == 0) return B_NO_ERROR; 
      }
   }

   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft)
   {
#ifdef TODO_IMPLEMNET_THIS
      if (msgRef() == NULL) return B_ERROR("Owner thread requested exit.");
      return _tcpSession ? _tcpSession->AddOutgoingMessage(msgRef) : B_NO_ERROR;
#endif
      return B_NO_ERROR;
   }

   ClientConnector * _master;
   const String _signaturePattern;
   ConstQueryFilterRef _queryFilter;
   bool _isActive;
   bool _isConnected;
   uint64 _reconnectTimeMicroseconds;
   MessageRef _discoveries;
};

ClientConnector :: ClientConnector(ICallbackMechanism * mechanism, const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalCriteria)
   : ICallbackSubscriber(mechanism)
{
   ConstQueryFilterRef filterRef(new StringQueryFilter(ZG_DISCOVERY_NAME_SYSTEMNAME, StringQueryFilter::OP_SIMPLE_WILDCARD_MATCH, systemNamePattern));
   if (optAdditionalCriteria()) filterRef.SetRef(new AndQueryFilter(filterRef, optAdditionalCriteria));
   _imp = new ClientConnectorImplementation(this, signaturePattern, filterRef);
}

ClientConnector :: ~ClientConnector() 
{
   MASSERT(IsActive() == false, "~ClientConnector:  You must call Stop() before deleting the ClientConnector object");
   delete _imp;
}

status_t ClientConnector :: ParseTCPPortFromMessage(const Message & msg, uint16 & retPort) const {return msg.FindInt16("port", retPort);}
status_t ClientConnector :: Start(uint64 reconnectTimeMicroseconds) {return _imp->Start(reconnectTimeMicroseconds);}
void ClientConnector :: Stop() {_imp->Stop();}
bool ClientConnector :: IsActive() const {return _imp->IsActive();}

void ClientConnector :: DispatchCallbacks(uint32 eventTypeBits)
{
printf("ClientConnector %p:  DispatchBits %x\n", this, eventTypeBits);
}

status_t ClientConnector :: SendOutgoingMessageToNetwork(const MessageRef & msg) {return _imp->SendOutgoingMessageToNetwork(msg);}

};  // end namespace zg

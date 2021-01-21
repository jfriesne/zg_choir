#include "zg/udp/UDPMulticastTransceiver.h"
#include "zg/udp/IUDPMulticastNotificationTarget.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for GetTransceiverMulticastAddresses
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/SignalMessageIOGateway.h"
#include "reflector/ReflectServer.h"
#include "system/DetectNetworkConfigChangesSession.h"
#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"

namespace zg {

enum {
   UDP_COMMAND_SEND_PACKET = 1969516660 // 'udpt' 
};

static const String UDP_NAME_PAYLOAD = "pay";

class MulticastUDPClientManagerSession;

class MulticastUDPSession : public AbstractReflectSession
{
public:
   MulticastUDPSession(const IPAddressAndPort & multicastIAP, MulticastUDPClientManagerSession * manager)
      : _multicastIAP(multicastIAP)
      , _manager(manager)
   {
      // empty
   }

   virtual ~MulticastUDPSession()
   {
      // empty
   }

   virtual ConstSocketRef CreateDefaultSocket();

   virtual DataIORef CreateDataIO(const ConstSocketRef & s)
   {
      UDPSocketDataIO * udpIO = new UDPSocketDataIO(s, false);
      DataIORef ret(udpIO);
      (void) udpIO->SetPacketSendDestination(_multicastIAP);
      return ret;
   }

   virtual void MessageReceivedFromSession(AbstractReflectSession & /*from*/, const MessageRef & msg, void * /*userData*/)
   {
      AddOutgoingMessage(msg);
   }

   virtual int32 DoInput(AbstractGatewayMessageReceiver &, uint32 maxBytes);

   virtual int32 DoOutput(uint32 maxBytes)
   {
      TCHECKPOINT;

      if (GetGateway()() == NULL) return -1;  // abort sessions that couldn't create a socket

      DataIO & udpIO = *GetGateway()()->GetDataIO()();
      Queue<MessageRef> & oq = GetGateway()()->GetOutgoingMessageQueue();
      uint32 ret = 0;
      while((oq.HasItems())&&(ret < maxBytes))
      {
         ByteBufferRef bufRef;
         if (oq.Head()()->FindFlat(UDP_NAME_PAYLOAD, bufRef).IsOK())
         {
            const int32 bytesSent = udpIO.Write(bufRef()->GetBuffer(), bufRef()->GetNumBytes());
            if (bytesSent > 0) 
            {
               ret += bytesSent;
               if (GetMaxLogLevel() >= MUSCLE_LOG_TRACE) LogTime(MUSCLE_LOG_TRACE, "MulticastUDPSession %p sent " INT32_FORMAT_SPEC " bytes of multicast-packet %s\n", this, bytesSent, _multicastIAP.ToString()());
            }
         }
         oq.RemoveHead();
      }
      return ret;
   }

   virtual void MessageReceivedFromGateway(const muscle::MessageRef&, void*) {/* empty */}

private:
   const IPAddressAndPort _multicastIAP;
   MulticastUDPClientManagerSession * _manager;
   
   ByteBufferRef _receiveBuffer;
};
DECLARE_REFTYPES(MulticastUDPSession);

// This class will set up new MulticastUDPSessions as necessary when the network config changes.
class MulticastUDPClientManagerSession : public AbstractReflectSession, public INetworkConfigChangesTarget
{
public:
   MulticastUDPClientManagerSession(UDPMulticastTransceiverImplementation * imp, const String & transmissionKey, bool enableReceive) 
      : _imp(imp)
      , _transmissionKey(transmissionKey)
      , _enableReceive(enableReceive)
   {
      // empty
   }

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      AbstractMessageIOGatewayRef ret(newnothrow SignalMessageIOGateway());
      if (ret() == NULL) WARN_OUT_OF_MEMORY;
      return ret;
   }

   virtual status_t AttachedToServer()
   {
      status_t ret;
      if (AbstractReflectSession::AttachedToServer().IsError(ret)) return ret;

      AddNewMulticastUDPSessions();
      return B_NO_ERROR;
   }

   virtual void AboutToDetachFromServer()
   {  
      EndExistingMulticastUDPSessions();
      AbstractReflectSession::AboutToDetachFromServer();
   }

   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & /*optInterfaceNames*/)
   {  
      LogTime(MUSCLE_LOG_DEBUG, "MulticastUDPClientManagerSession:  NetworkInterfacesChanged!  Recreating MulticastUDPSessions...\n");
      EndExistingMulticastUDPSessions();
      AddNewMulticastUDPSessions();
   }

   virtual void ComputerIsAboutToSleep();
   virtual void ComputerJustWokeUp();
 
   void MulticastUDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetBytes);

   virtual void MessageReceivedFromGateway(const MessageRef &, void *);

   bool IsEnableReceive() const {return _enableReceive;}

private:
   void EndExistingMulticastUDPSessions()
   {
      // First, tell any existing MulticastUDPSessions to go away
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {  
         MulticastUDPSession * lds = dynamic_cast<MulticastUDPSession *>(iter.GetValue()());
         if (lds) lds->EndSession();
      }
   }

   void AddNewMulticastUDPSessions()
   {
      // Now set up new MulticastUDPSessions based on our current network config
      Queue<IPAddressAndPort> q;
      if (GetTransceiverMulticastAddresses(q, _transmissionKey).IsOK())
      {
         for (uint32 i=0; i<q.GetNumItems(); i++)
         {
            // FogBugz #5617:  Use a different socket for each IP address, to avoid Mac routing problems
            status_t ret;
            MulticastUDPSessionRef ldsRef(newnothrow MulticastUDPSession(q[i], this));
                 if (ldsRef() == NULL) WARN_OUT_OF_MEMORY; 
            else if (AddNewSession(ldsRef).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Could not create transceiver session for [%s] [%s]\n", q[i].ToString()(), ret());
         }
      }
   }

   UDPMulticastTransceiverImplementation * _imp;
   const String _transmissionKey;
   const bool _enableReceive;
};

int32 MulticastUDPSession :: DoInput(AbstractGatewayMessageReceiver &, uint32 maxBytes)
{
   PacketDataIO & udpIO = *(dynamic_cast<PacketDataIO *>(GetGateway()()->GetDataIO()()));

   uint32 ret = 0;
   while(ret < maxBytes)
   {
      IPAddressAndPort sourceLoc;
      const int32 bytesRead = udpIO.ReadFrom(_receiveBuffer()->GetBuffer(), _receiveBuffer()->GetNumBytes(), sourceLoc);
      if (bytesRead > 0)
      {
         if (GetMaxLogLevel() >= MUSCLE_LOG_TRACE) LogTime(MUSCLE_LOG_TRACE, "MulticastUDPSession %p read " INT32_FORMAT_SPEC " bytes of multicast-reply data from %s\n", this, bytesRead, sourceLoc.ToString()());

         ByteBufferRef newReceiveBuffer = GetByteBufferFromPool(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);  // get ready for our next received-packet
         if (newReceiveBuffer())
         {
            newReceiveBuffer.SwapContents(_receiveBuffer);
            (void) newReceiveBuffer()->SetNumBytes(bytesRead, true);
            _manager->MulticastUDPPacketReceived(sourceLoc, newReceiveBuffer);
         }

         ret += bytesRead;
      }
      else break;
   }
   return ret;
}

ConstSocketRef MulticastUDPSession :: CreateDefaultSocket()
{
   _receiveBuffer = GetByteBufferFromPool(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);
   if (_receiveBuffer())
   {
      ConstSocketRef udpSocket = CreateUDPSocket();
      if ((udpSocket())&&(BindUDPSocket(udpSocket, _multicastIAP.GetPort(), NULL, invalidIP, true).IsOK())&&(SetSocketBlockingEnabled(udpSocket, false).IsOK()))
      {
         if ((_manager->IsEnableReceive())&&(AddSocketToMulticastGroup(udpSocket, _multicastIAP.GetIPAddress()).IsError())) return ConstSocketRef();
         return udpSocket;
      }
   }
   return ConstSocketRef();
}

enum {
   UDP_EVENT_TYPE_PACKETRECEIVED = 0,
   UDP_EVENT_TYPE_SLEEP,
   UDP_EVENT_TYPE_AWAKE,
   NUM_UDP_EVENT_TYPES
};

class UDPMulticastTransceiverImplementation : private Thread
{
public:
   UDPMulticastTransceiverImplementation(UDPMulticastTransceiver & master) : _master(master)
   {
      // empty
   }

   virtual ~UDPMulticastTransceiverImplementation()
   {
      Stop();  // paranoia
   }

   // Called by the main thread
   status_t Start() 
   {
      Stop();  // paranoia
      return StartInternalThread();
   }

   // Called by the main thread
   void Stop() 
   {
      (void) ShutdownInternalThread();
      _ioThreadPendingUpdates.Clear();
      _mainThreadPendingUpdates.Clear();
   }

   // Called by the main thread
   Hashtable<IPAddressAndPort, Queue<ByteBufferRef> > & SwapUpdateBuffers()
   {
      MutexGuard mg(_mutex);
      _mainThreadPendingUpdates.SwapContents(_ioThreadPendingUpdates);
      return _mainThreadPendingUpdates;  // returning a read/write reference so the calling code can iterate it and then clear it
   }

   // Called by the main thread
   status_t SendMulticastPacket(const ByteBufferRef & payloadBytes)
   {
      MessageRef msg = GetMessageFromPool(UDP_COMMAND_SEND_PACKET);
      if (msg() == NULL) return B_OUT_OF_MEMORY;

      status_t ret;
      return (msg()->AddFlat(UDP_NAME_PAYLOAD, payloadBytes).IsOK(ret)) ? SendMessageToInternalThread(msg) : ret;
   }

protected:
   // Called in internal I/O thread
   virtual void InternalThreadEntry()
   {
      ReflectServer server;

      // We need to keep track of when the set of available network interfaces changes
      status_t ret;
      DetectNetworkConfigChangesSession dnccs;
      if (server.AddNewSession(AbstractReflectSessionRef(&dnccs, false)).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "MulticastUDPServer:  Couldn't add DetectNetworkChangesSession! [%s]\n", ret());
         return;
      }

      // We need to watch our notification-socket to know when it is time to exit
      MulticastUDPClientManagerSession cdms(this, _master._transmissionKey, (_master._perSenderMaxBacklogDepth > 0));
      if (server.AddNewSession(AbstractReflectSessionRef(&cdms, false), GetInternalThreadWakeupSocket()).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "UDPMulticastTransceiverImplementation:  Couldn't add MulticastUDPClientManagerSession! [%s]\n", ret());
         return;
      }

      (void) server.ServerProcessLoop();
      server.Cleanup();
   }

   // Called in internal I/O thread
   void MulticastUDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetData)
   {
      bool sendSignal = false;
      {
         MutexGuard mg(_mutex);  // critical section starts here

         const bool wasEmpty = _ioThreadPendingUpdates.IsEmpty();
         Queue<ByteBufferRef> * q = _ioThreadPendingUpdates.GetOrPut(sourceIAP);
         if (q)
         {
            (void) q->AddTail(packetData);
            if (q->GetNumItems() > _master._perSenderMaxBacklogDepth) (void) q->RemoveHead();
            if (q->IsEmpty()) (void) _ioThreadPendingUpdates.Remove(sourceIAP);  // semi-paranoia
            if ((wasEmpty)&&(_ioThreadPendingUpdates.HasItems())) sendSignal = true;
         }
      }

      if (sendSignal) _master.RequestCallbackInDispatchThread(1<<UDP_EVENT_TYPE_PACKETRECEIVED);
   }

   // Called in internal I/O thread
   void ReportSleepNotification(bool isAboutToSleep)
   {
      const int setBitIdx = isAboutToSleep ? UDP_EVENT_TYPE_SLEEP : UDP_EVENT_TYPE_AWAKE;
      const int clrBitIdx = isAboutToSleep ? UDP_EVENT_TYPE_AWAKE : UDP_EVENT_TYPE_SLEEP;
      _master.RequestCallbackInDispatchThread(1<<setBitIdx, 1<<clrBitIdx);  // the two bits are mutually exclusive
   }

private:
   friend class MulticastUDPClientManagerSession;

   // called in internal thread
   status_t HandleMessagesFromOwner(MulticastUDPClientManagerSession * managerSession)
   {
      MessageRef msg;
      while(WaitForNextMessageFromOwner(msg, 0) >= 0)
      {
         if (msg()) managerSession->BroadcastToAllSessions(msg);
               else return B_ERROR;  // time for this thread to go away!
      }
      return B_NO_ERROR;
   }

   UDPMulticastTransceiver & _master;

   Mutex _mutex;
   Hashtable<IPAddressAndPort, Queue<ByteBufferRef> > _ioThreadPendingUpdates;   // access to this must be serialized via _mutex
   Hashtable<IPAddressAndPort, Queue<ByteBufferRef> > _mainThreadPendingUpdates; // access to this must be serialized via _mutex
};

void MulticastUDPClientManagerSession :: MessageReceivedFromGateway(const MessageRef & /*dummySignalMessage*/, void *)
{
   if (_imp->HandleMessagesFromOwner(this).IsError()) EndServer();
}

void MulticastUDPClientManagerSession :: ComputerIsAboutToSleep() {_imp->ReportSleepNotification(true);}
void MulticastUDPClientManagerSession :: ComputerJustWokeUp()     {_imp->ReportSleepNotification(false);}
void MulticastUDPClientManagerSession :: MulticastUDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetBytes) {_imp->MulticastUDPPacketReceived(sourceIAP, packetBytes);}

UDPMulticastTransceiver :: UDPMulticastTransceiver(ICallbackMechanism * mechanism) 
   : ICallbackSubscriber(mechanism)
   , _perSenderMaxBacklogDepth(1)
   , _isActive(false)
{
   _imp = new UDPMulticastTransceiverImplementation(*this);
}

UDPMulticastTransceiver :: ~UDPMulticastTransceiver()
{
   while(_targets.HasItems()) _targets.GetFirstKeyWithDefault()->SetUDPMulticastTransceiver(NULL);  // un-register everyone to avoid dangling pointer issues
   delete _imp;
}

status_t UDPMulticastTransceiver :: Start(const String & transmissionKey, uint32 perSenderMaxBacklogDepth)
{
   const String oldTransmissionKey          = _transmissionKey;
   const uint32 oldPerSenderMaxBacklogDepth = _perSenderMaxBacklogDepth;

   Stop();

   _transmissionKey          = transmissionKey;
   _perSenderMaxBacklogDepth = perSenderMaxBacklogDepth;

   status_t ret;
   if (_imp->Start().IsOK(ret)) 
   {
      _isActive = true;
      return B_NO_ERROR;
   }

   // roll back!
   _transmissionKey          = oldTransmissionKey;
   _perSenderMaxBacklogDepth = oldPerSenderMaxBacklogDepth;
   return ret;
}

void UDPMulticastTransceiver :: Stop()
{
   _imp->Stop();
   _transmissionKey.Clear();
   _perSenderMaxBacklogDepth = 1;
   _isActive = false;
}

// Called in the main thread
void UDPMulticastTransceiver :: MainThreadNotifyAllOfSleepChange(bool isAboutToSleep)
{
   for (HashtableIterator<IUDPMulticastNotificationTarget *, Void> iter(_targets); iter.HasData(); iter++)
   {
      IUDPMulticastNotificationTarget * t = iter.GetKey();
      if (isAboutToSleep) t->ComputerIsAboutToSleep();
                     else t->ComputerJustWokeUp();
   }
}

// Called in the main thread
void UDPMulticastTransceiver :: DispatchCallbacks(uint32 eventTypeBits)
{
   if (eventTypeBits & (1<<UDP_EVENT_TYPE_PACKETRECEIVED))
   {
      Hashtable<IPAddressAndPort, Queue<ByteBufferRef> > & mainThreadPendingUpdates = _imp->SwapUpdateBuffers();  // safely swaps the contents of _ioThreadPendingUpdates and _mainThreadPendingUpdates

      // Notify all the targets of incoming packet
      for (HashtableIterator<IPAddressAndPort, Queue<ByteBufferRef> > iter(mainThreadPendingUpdates); iter.HasData(); iter++) 
      {
         const IPAddressAndPort & sourceIAP = iter.GetKey();
         const Queue<ByteBufferRef> & bbq = iter.GetValue();
         for (uint32 i=0; i<bbq.GetNumItems(); i++)
         {
            const ByteBufferRef & b = bbq[i];
            for (HashtableIterator<IUDPMulticastNotificationTarget *, Void> subIter(_targets); subIter.HasData(); subIter++)
               subIter.GetKey()->MulticastUDPPacketReceived(sourceIAP, b);
         }
      }
      mainThreadPendingUpdates.Clear();
   }

   if (eventTypeBits & (1<<UDP_EVENT_TYPE_SLEEP)) MainThreadNotifyAllOfSleepChange(true);
   if (eventTypeBits & (1<<UDP_EVENT_TYPE_AWAKE)) MainThreadNotifyAllOfSleepChange(false);
}

void IUDPMulticastNotificationTarget :: SetUDPMulticastTransceiver(UDPMulticastTransceiver * transceiver)
{
   if (transceiver != _multicastTransceiver)
   {
      if (_multicastTransceiver) _multicastTransceiver->UnregisterTarget(this);
      _multicastTransceiver = transceiver;
      if (_multicastTransceiver) _multicastTransceiver->RegisterTarget(this);
   }
}

void UDPMulticastTransceiver :: RegisterTarget(IUDPMulticastNotificationTarget * newTarget)
{
   (void) _targets.PutWithDefault(newTarget);
}

void UDPMulticastTransceiver :: UnregisterTarget(IUDPMulticastNotificationTarget * target) 
{
   (void) _targets.Remove(target);
}

status_t UDPMulticastTransceiver :: SendMulticastPacket(const ByteBufferRef & payloadBytes)
{
   return _isActive ? _imp->SendMulticastPacket(payloadBytes) : B_BAD_OBJECT;  // no point queuing up outgoing data if our internal thread isn't running
}

status_t IUDPMulticastNotificationTarget :: SendMulticastPacket(const ByteBufferRef & payloadBytes)
{
   return _multicastTransceiver ? _multicastTransceiver->SendMulticastPacket(payloadBytes) : B_BAD_OBJECT;
}

};  // end namespace zg

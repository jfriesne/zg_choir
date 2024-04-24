#include "zg/udp/UDPMulticastTransceiver.h"
#include "zg/udp/IUDPMulticastNotificationTarget.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for GetTransceiverMulticastAddresses
#include "dataio/SimulatedMulticastDataIO.h"
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/SignalMessageIOGateway.h"
#include "reflector/ReflectServer.h"
#include "regex/StringMatcher.h"
#include "system/DetectNetworkConfigChangesSession.h"
#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"

namespace zg {

enum {
   UDP_COMMAND_SEND_MULTICAST_PACKET = 1969516660, // 'udpt'
   UDP_COMMAND_SEND_UNICAST_PACKET
};

static const String UDP_NAME_PAYLOAD = "pay";
static const String UDP_NAME_ADDRESS = "add";

class MulticastUDPClientManagerSession;

class MulticastUDPSession : public AbstractReflectSession
{
public:
   MulticastUDPSession(bool useSimulatedMulticast, const IPAddressAndPort & multicastIAP, MulticastUDPClientManagerSession * manager)
      : _multicastIAP(multicastIAP)
      , _useSimulatedMulticast(useSimulatedMulticast)
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
      PacketDataIO * udpIO;
      if (_useSimulatedMulticast) udpIO = new SimulatedMulticastDataIO(_multicastIAP);
                             else udpIO = new UDPSocketDataIO(s, false);

      DataIORef ret(udpIO);
      (void) udpIO->SetPacketSendDestination(_multicastIAP);
      return ret;
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

      PacketDataIO & udpIO = dynamic_cast<PacketDataIO &>(*GetGateway()()->GetDataIO()());
      Queue<MessageRef> & oq = GetGateway()()->GetOutgoingMessageQueue();
      uint32 ret = 0;
      while((oq.HasItems())&&(ret < maxBytes))
      {
         const Message & m = *oq.Head()();

         ByteBufferRef bufRef;
         if (m.FindFlat(UDP_NAME_PAYLOAD, bufRef).IsOK())
         {
            switch(m.what)
            {
               case UDP_COMMAND_SEND_MULTICAST_PACKET:
               {
                  const int32 bytesSent = udpIO.Write(bufRef()->GetBuffer(), bufRef()->GetNumBytes()).GetByteCount();
                  if (bytesSent > 0)
                  {
                     ret += bytesSent;
                     if (GetMaxLogLevel() >= MUSCLE_LOG_TRACE) LogTime(MUSCLE_LOG_TRACE, "MulticastUDPSession %p sent " INT32_FORMAT_SPEC " bytes of multicast-packet %s\n", this, bytesSent, _multicastIAP.ToString()());
                  }
               }
               break;

               case UDP_COMMAND_SEND_UNICAST_PACKET:
               {
                  IPAddressAndPort targetAddress;
                  if (m.FindFlat(UDP_NAME_ADDRESS, targetAddress).IsOK())
                  {
                     const int32 bytesSent = udpIO.WriteTo(bufRef()->GetBuffer(), bufRef()->GetNumBytes(), targetAddress).GetByteCount();
                     if (bytesSent > 0)
                     {
                        ret += bytesSent;
                        if (GetMaxLogLevel() >= MUSCLE_LOG_TRACE) LogTime(MUSCLE_LOG_TRACE, "MulticastUDPSession %p sent " INT32_FORMAT_SPEC " bytes of unicast-packet %s\n", this, bytesSent, targetAddress.ToString()());
                     }
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "MulticastUDPSession:  No target address found in send-unicast Message!\n");
               }
               break;

               default:
                  LogTime(MUSCLE_LOG_ERROR, "MulticastUDPSession:  Unknown command code " UINT32_FORMAT_SPEC "\n", m.what);
               break;
            }
         }
         (void) oq.RemoveHead();
      }
      return ret;
   }

   virtual void MessageReceivedFromGateway(const muscle::MessageRef&, void*) {/* empty */}

private:
   const IPAddressAndPort _multicastIAP;
   const bool _useSimulatedMulticast;
   MulticastUDPClientManagerSession * _manager;

   ByteBufferRef _receiveBuffer;
};
DECLARE_REFTYPES(MulticastUDPSession);

// This class will set up new MulticastUDPSessions as necessary when the network config changes.
class MulticastUDPClientManagerSession : public AbstractReflectSession, public INetworkConfigChangesTarget
{
public:
   MulticastUDPClientManagerSession(UDPMulticastTransceiverImplementation * imp, const String & transmissionKey, bool enableReceive, uint32 multicastBehavior, const String & nicNameFilter)
      : _imp(imp)
      , _transmissionKey(transmissionKey)
      , _enableReceive(enableReceive)
      , _multicastBehavior(multicastBehavior)
      , _nicNameFilter(nicNameFilter)
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

   void UDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetBytes);

   virtual void MessageReceivedFromGateway(const MessageRef &, void *);

   bool IsEnableReceive() const {return _enableReceive;}

   void HandleMessageFromOwner(const MessageRef & msg)
   {
      switch(msg()->what)
      {
         case UDP_COMMAND_SEND_MULTICAST_PACKET:
            BroadcastToAllSessionsOfType<MulticastUDPSession>(msg);
         break;

         case UDP_COMMAND_SEND_UNICAST_PACKET:
         {
            IPAddressAndPort targetAddress;
            if (msg()->FindFlat(UDP_NAME_ADDRESS, targetAddress).IsOK())
            {
               MulticastUDPSession * mus = _udpSessions[targetAddress.GetIPAddress().GetInterfaceIndex()]();
               if (mus == NULL) mus = FindFirstSessionOfType<MulticastUDPSession>();   // if we can't find a session matching the target-addresses interface-index, we'll fall back to sending from anywhere
               if (mus) mus->MessageReceivedFromSession(*this, msg, NULL);
                   else LogTime(MUSCLE_LOG_ERROR, "UDPMulticastTransceiverImplementation::HandleMessagesFromOwner():  Couldn't find a MulticastUDPSession to send unicast packet to %s!\n", targetAddress.ToString()());
            }
            else LogTime(MUSCLE_LOG_ERROR, "UDPMulticastTransceiverImplementation:  No target address specified for unicast packet!\n");
         }
         break;

         default:
            LogTime(MUSCLE_LOG_ERROR, "UDPMulticastTransceiverImplementation::HandleMessagesFromOwner():  Unknown what-code " UINT32_FORMAT_SPEC "\n", msg()->what);
         break;
      }
   }

private:
   void EndExistingMulticastUDPSessions()
   {
      // First, tell any existing MulticastUDPSessions to go away
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         MulticastUDPSession * lds = dynamic_cast<MulticastUDPSession *>(iter.GetValue()());
         if (lds) lds->EndSession();
      }
      _udpSessions.Clear();
   }

   void AddNewMulticastUDPSessions()
   {
      const StringMatcher sm(_nicNameFilter);

      // Now set up new MulticastUDPSessions based on our current network config
      Hashtable<IPAddressAndPort, bool> q;
      if (GetTransceiverMulticastAddresses(q, _transmissionKey, _nicNameFilter.HasChars() ? &sm : NULL).IsOK())
      {
         for (HashtableIterator<IPAddressAndPort, bool> iter(q); iter.HasData(); iter++)
         {
            IPAddressAndPort iap = iter.GetKey();
            const bool isWiFi = iter.GetValue();

            bool useSimulatedMulticast;
            switch(_multicastBehavior)
            {
               case ZG_MULTICAST_BEHAVIOR_STANDARD_ONLY:  useSimulatedMulticast = false;  break;
               case ZG_MULTICAST_BEHAVIOR_SIMULATED_ONLY: useSimulatedMulticast = true;   break;
               case ZG_MULTICAST_BEHAVIOR_AUTO: default:  useSimulatedMulticast = isWiFi; break;
            }
            if (useSimulatedMulticast) iap.SetPort(iap.GetPort()+100);  // just to keep SimulatedMulticastDataIO control-packets from leaking into the user's standard-multicast receivers

            // Use a different socket for each IP address, to avoid Mac routing problems
            MulticastUDPSessionRef msRef(new MulticastUDPSession(useSimulatedMulticast, iap, this));

            status_t ret;
            if (AddNewSession(msRef).IsOK(ret))
            {
               const int iidx = iap.GetIPAddress().GetInterfaceIndex();
               if (_udpSessions.ContainsKey(iidx)) LogTime(MUSCLE_LOG_CRITICALERROR, "MulticastUDPClientManagerSession:  Multiple UDP sessions have the same interface index %i!\n", iidx);
               (void) _udpSessions.Put(iidx, msRef);
            }
            else LogTime(MUSCLE_LOG_ERROR, "Could not create %s-transceiver session for [%s] [%s]\n", useSimulatedMulticast?"simulated-multicast":"multicast", iap.ToString()(), ret());
         }
      }
   }

   UDPMulticastTransceiverImplementation * _imp;
   const String _transmissionKey;
   const bool _enableReceive;
   const uint32 _multicastBehavior;
   const String _nicNameFilter;

   Hashtable<int, MulticastUDPSessionRef> _udpSessions;  // interface index -> sessionRef
};

io_status_t MulticastUDPSession :: DoInput(AbstractGatewayMessageReceiver &, uint32 maxBytes)
{
   PacketDataIO * pUdpIO = dynamic_cast<PacketDataIO *>(GetGateway()()->GetDataIO()());
   if (pUdpIO == NULL) return B_BAD_OBJECT;  // paranoia

   PacketDataIO & udpIO = *pUdpIO;

   uint32 ret = 0;
   while(ret < maxBytes)
   {
      IPAddressAndPort sourceLoc;
      const int32 bytesRead = udpIO.ReadFrom(_receiveBuffer()->GetBuffer(), _receiveBuffer()->GetNumBytes(), sourceLoc).GetByteCount();
      if (bytesRead > 0)
      {
         if (GetMaxLogLevel() >= MUSCLE_LOG_TRACE) LogTime(MUSCLE_LOG_TRACE, "MulticastUDPSession %p read " INT32_FORMAT_SPEC " bytes of multicast-reply data from %s\n", this, bytesRead, sourceLoc.ToString()());

         if (_manager->IsEnableReceive())  // this could return false, e.g. if we are using a SimulatedMulticastDataIO
         {
            ByteBufferRef newReceiveBuffer = GetByteBufferFromPool(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);  // get ready for our next received-packet
            if (newReceiveBuffer())
            {
               newReceiveBuffer.SwapContents(_receiveBuffer);
               (void) newReceiveBuffer()->SetNumBytes(bytesRead, true);
               _manager->UDPPacketReceived(sourceLoc, newReceiveBuffer);
            }
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
      if (_useSimulatedMulticast) return GetInvalidSocket();  // the SimulatedMulticastDataIO will create and use its own socket(s), so don't waste time creating one here
      else
      {
         ConstSocketRef udpSocket = CreateUDPSocket();
         if ((udpSocket())&&(BindUDPSocket(udpSocket, _multicastIAP.GetPort(), NULL, invalidIP, true).IsOK())&&(SetSocketBlockingEnabled(udpSocket, false).IsOK()))
         {
            if ((_manager->IsEnableReceive())&&(AddSocketToMulticastGroup(udpSocket, _multicastIAP.GetIPAddress()).IsError())) return ConstSocketRef();
            return udpSocket;
         }
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
   UDPMulticastTransceiverImplementation(UDPMulticastTransceiver & master) : _master(master), _multicastBehavior(ZG_MULTICAST_BEHAVIOR_AUTO)
   {
      // empty
   }

   virtual ~UDPMulticastTransceiverImplementation()
   {
      Stop();  // paranoia
   }

   // Called by the main thread
   status_t Start(uint32 multicastBehavior, const String & nicNameFilter)
   {
      Stop();  // paranoia
      _multicastBehavior = multicastBehavior;
      _nicNameFilter     = nicNameFilter;
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
      DECLARE_MUTEXGUARD(_mutex);
      _mainThreadPendingUpdates.SwapContents(_ioThreadPendingUpdates);
      return _mainThreadPendingUpdates;  // returning a read/write reference so the calling code can iterate it and then clear it
   }

   // Called by the main thread
   status_t SendMulticastPacket(const ByteBufferRef & payloadBytes)
   {
      MessageRef msg = GetMessageFromPool(UDP_COMMAND_SEND_MULTICAST_PACKET);
      if (msg() == NULL) return B_OUT_OF_MEMORY;

      status_t ret;
      return (msg()->AddFlat(UDP_NAME_PAYLOAD, payloadBytes).IsOK(ret)) ? SendMessageToInternalThread(msg) : ret;
   }

   // Called by the main thread
   status_t SendUnicastPacket(const IPAddressAndPort & targetAddress, const ByteBufferRef & payloadBytes)
   {
      MessageRef msg = GetMessageFromPool(UDP_COMMAND_SEND_UNICAST_PACKET);
      if (msg() == NULL) return B_OUT_OF_MEMORY;

      status_t ret;
      return ((msg()->AddFlat(UDP_NAME_PAYLOAD, payloadBytes).IsOK(ret))&&(msg()->AddFlat(UDP_NAME_ADDRESS, targetAddress).IsOK(ret))) ? SendMessageToInternalThread(msg) : ret;
   }

protected:
   // Called in internal I/O thread
   virtual void InternalThreadEntry()
   {
      ReflectServer server;

      // We need to keep track of when the set of available network interfaces changes
      status_t ret;
      DetectNetworkConfigChangesSession dnccs;
      if (server.AddNewSession(DummyAbstractReflectSessionRef(dnccs)).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "MulticastUDPServer:  Couldn't add DetectNetworkChangesSession! [%s]\n", ret());
         return;
      }

      // We need to watch our notification-socket to know when it is time to exit
      MulticastUDPClientManagerSession cdms(this, _master._transmissionKey, (_master._perSenderMaxBacklogDepth > 0), _multicastBehavior, _nicNameFilter);
      if (server.AddNewSession(DummyAbstractReflectSessionRef(cdms), GetInternalThreadWakeupSocket()).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "UDPMulticastTransceiverImplementation:  Couldn't add MulticastUDPClientManagerSession! [%s]\n", ret());
         return;
      }

      (void) server.ServerProcessLoop();
      server.Cleanup();
   }

   // Called in internal I/O thread
   void UDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetData)
   {
      bool sendSignal = false;
      {
         DECLARE_MUTEXGUARD(_mutex);  // critical section starts here

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
      while(WaitForNextMessageFromOwner(msg, 0).IsOK())
      {
         if (msg())
         {
            managerSession->HandleMessageFromOwner(msg);
         }
         else return B_ERROR;  // time for this thread to go away!
      }
      return B_NO_ERROR;
   }

   UDPMulticastTransceiver & _master;

   Mutex _mutex;
   Hashtable<IPAddressAndPort, Queue<ByteBufferRef> > _ioThreadPendingUpdates;   // access to this must be serialized via _mutex
   Hashtable<IPAddressAndPort, Queue<ByteBufferRef> > _mainThreadPendingUpdates; // access to this must be serialized via _mutex
   uint32 _multicastBehavior;
   String _nicNameFilter;
};

void MulticastUDPClientManagerSession :: MessageReceivedFromGateway(const MessageRef & /*dummySignalMessage*/, void *)
{
   if (_imp->HandleMessagesFromOwner(this).IsError()) EndServer();
}

void MulticastUDPClientManagerSession :: ComputerIsAboutToSleep() {_imp->ReportSleepNotification(true);}
void MulticastUDPClientManagerSession :: ComputerJustWokeUp()     {_imp->ReportSleepNotification(false);}
void MulticastUDPClientManagerSession :: UDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetBytes) {_imp->UDPPacketReceived(sourceIAP, packetBytes);}

UDPMulticastTransceiver :: UDPMulticastTransceiver(ICallbackMechanism * mechanism)
   : ICallbackSubscriber(mechanism)
   , _perSenderMaxBacklogDepth(1)
   , _multicastBehavior(ZG_MULTICAST_BEHAVIOR_AUTO)
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
   if (_imp->Start(_multicastBehavior, _nicNameFilter).IsOK(ret))
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
               subIter.GetKey()->UDPPacketReceived(sourceIAP, b);
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

status_t UDPMulticastTransceiver :: SendUnicastPacket(const IPAddressAndPort & targetAddress, const ByteBufferRef & payloadBytes)
{
   return _isActive ? _imp->SendUnicastPacket(targetAddress, payloadBytes) : B_BAD_OBJECT;  // no point queuing up outgoing data if our internal thread isn't running
}

status_t IUDPMulticastNotificationTarget :: SendMulticastPacket(const ByteBufferRef & payloadBytes)
{
   return _multicastTransceiver ? _multicastTransceiver->SendMulticastPacket(payloadBytes) : B_BAD_OBJECT;
}

status_t IUDPMulticastNotificationTarget :: SendUnicastPacket(const IPAddressAndPort & targetAddress, const ByteBufferRef & payloadBytes)
{
   return _multicastTransceiver ? _multicastTransceiver->SendUnicastPacket(targetAddress, payloadBytes) : B_BAD_OBJECT;
}

};  // end namespace zg

#include "reflector/StorageReflectConstants.h"
#include "system/SystemInfo.h"
#include "system/DetectNetworkConfigChangesSession.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "zg/discovery/server/DiscoveryServerSession.h"
#include "zg/discovery/server/IDiscoveryServerSessionController.h"

namespace zg {

class DiscoveryDetectNetworkConfigChangesSession : public DetectNetworkConfigChangesSession
{
public:
   explicit DiscoveryDetectNetworkConfigChangesSession(DiscoveryServerSession * master)
      : _master(master)
   {
      // empty
   }

   virtual void EndSession() {_master = NULL; DetectNetworkConfigChangesSession::EndSession();}

   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames)
   {
      DetectNetworkConfigChangesSession::NetworkInterfacesChanged(optInterfaceNames);
      if (_master) _master->NetworkInterfacesChanged(optInterfaceNames);
   }

private:
   DiscoveryServerSession * _master;
};

DiscoveryServerSession :: DiscoveryServerSession(IDiscoveryServerSessionController & master, uint16 discoveryPort)
   : _discoveryPort(discoveryPort)
   , _master(&master)
{
   // empty
}

DiscoveryServerSession :: ~DiscoveryServerSession()
{
   // empty
}

void DiscoveryServerSession :: NetworkInterfacesChanged(const Hashtable<String, Void> & /*optInterfaceNames*/)
{
   // Replace our DataIO with a new one that will use the new network interfaces
   AbstractMessageIOGateway * gw = IsAttachedToServer() ? GetGateway()() : NULL;
   if (gw)
   {
      ConstSocketRef newSock = CreateDefaultSocket();
      if (newSock())
      {
         DataIORef dio = CreateDataIO(newSock);
         if (dio()) gw->SetDataIO(dio);
      }
   }
}

status_t DiscoveryServerSession :: AttachedToServer()
{
   _watchInterfacesSession.SetRef(new DiscoveryDetectNetworkConfigChangesSession(this));

   status_t ret;
   if (AddNewSession(_watchInterfacesSession).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "DiscoveryServerSession:  Error adding interfaces-change-notifier session! [%s]\n", ret());
      _watchInterfacesSession.Reset();
   }

   _receiveBuffer = GetByteBufferFromPool(2048);
   return _receiveBuffer() ? AbstractReflectSession::AttachedToServer() : B_OUT_OF_MEMORY;
}

void DiscoveryServerSession :: AboutToDetachFromServer()
{
   if (_watchInterfacesSession())
   {
      _watchInterfacesSession()->EndSession(); // this will cause it to forget its pointer to us, also
      _watchInterfacesSession.Reset();         // which is important since we're about to go away
   }
   AbstractReflectSession::AboutToDetachFromServer();
}

ConstSocketRef DiscoveryServerSession :: CreateDefaultSocket()
{
   uint32 numAddedGroups = 0;
   ConstSocketRef udpSocket = CreateUDPSocket();
   Hashtable<IPAddressAndPort, bool> mcastIAPs;
   status_t ret;

   if ((udpSocket())
    && (GetDiscoveryMulticastAddresses(mcastIAPs, _discoveryPort).IsOK(ret))
    && (mcastIAPs.HasItems())
    && (BindUDPSocket(udpSocket, mcastIAPs.GetFirstKey()->GetPort(), NULL, invalidIP, true).IsOK()))
   {
      (void) SetSocketBlockingEnabled(udpSocket, false);

      for (HashtableIterator<IPAddressAndPort, bool> iter(mcastIAPs); iter.HasData(); iter++)
      {
         const IPAddressAndPort & iap = iter.GetKey();

         if (AddSocketToMulticastGroup(udpSocket, iap.GetIPAddress()).IsOK()) numAddedGroups++;
         else
         {
            LogTime(MUSCLE_LOG_ERROR, "DiscoveryServerSession::CreateDefaultSocket:  Unable to add socket to multicast group [%s]!\n", Inet_NtoA(iap.GetIPAddress())());
         }
      }
   }

   return (numAddedGroups>0) ? udpSocket : ConstSocketRef();
}

uint64 DiscoveryServerSession :: GetPulseTime(const PulseArgs & args)
{
   uint64 ret = AbstractReflectSession::GetPulseTime(args);
   if (_queuedOutputData.HasItems()) ret = muscleMin(ret, _queuedOutputData.GetFirstValue()->GetSendTime());
   return ret;
}

void DiscoveryServerSession :: Pulse(const PulseArgs & args)
{
   AbstractReflectSession::Pulse(args);
   while((_queuedOutputData.HasItems())&&(_queuedOutputData.GetFirstValue()->GetSendTime() <= args.GetCallbackTime()))
   {
      (void) _outputData.Put(*_queuedOutputData.GetFirstKey(), *_queuedOutputData.GetFirstValue());
      (void) _queuedOutputData.RemoveFirst();
   }
}

io_status_t DiscoveryServerSession :: DoInput(AbstractGatewayMessageReceiver & /*receiver*/, uint32 maxBytes)
{
   const ConstSocketRef & s = GetSessionReadSelectSocket();
   if ((s() == NULL)||(_master == NULL)) return B_BAD_OBJECT;

   const uint64 now = GetRunTime64();
   uint32 ret = 0;
   while(ret < maxBytes)
   {
      IPAddress sourceIP;
      uint16 sourcePort;
      const int32 bytesRead = ReceiveDataUDP(s, _receiveBuffer()->GetBuffer(), _receiveBuffer()->GetNumBytes(), false, &sourceIP, &sourcePort).GetByteCount();
      if (bytesRead > 0)
      {
         const IPAddressAndPort iap(sourceIP, sourcePort);
         LogTime(MUSCLE_LOG_TRACE, "Received " INT32_FORMAT_SPEC " bytes of discovery query data from %s\n", bytesRead, iap.ToString()());

         MessageRef msg = GetMessageFromPool();
         if ((msg())&&(msg()->UnflattenFromBytes(_receiveBuffer()->GetBuffer(), bytesRead).IsOK()))
         {
            const uint64 pongDelayMicros = _master->HandleDiscoveryPing(msg, iap);
            if (pongDelayMicros != MUSCLE_TIME_NEVER)
            {
               const UDPReply * r = _queuedOutputData.Get(iap);
               (void) _queuedOutputData.Put(iap, UDPReply(muscleMin(r?r->GetSendTime():MUSCLE_TIME_NEVER, (pongDelayMicros>0)?(now+pongDelayMicros):0), msg));
               InvalidatePulseTime();
            }
         }
         ret += bytesRead;
      }
      else break;
   }

   return ret;
}

void DiscoveryServerSession :: MessageReceivedFromSession(AbstractReflectSession &, const MessageRef &, void *)
{
   // deliberately empty
}

io_status_t DiscoveryServerSession :: DoOutput(uint32 maxBytes)
{
   const ConstSocketRef & s = GetSessionWriteSelectSocket();
   if (s() == NULL) return B_BAD_OBJECT;

   uint32 ret = 0;
   while((_outputData.HasItems())&&(ret < maxBytes))
   {
      const UDPReply & next = *_outputData.GetFirstValue();
      ByteBufferRef bufRef = GetFlattenedByteBufferFromPool(*next.GetData()());
      if (bufRef())
      {
         const IPAddressAndPort & replyTarget = *_outputData.GetFirstKey();
         const int32 bytesSent = SendDataUDP(s, bufRef()->GetBuffer(), bufRef()->GetNumBytes(), false, replyTarget.GetIPAddress(), replyTarget.GetPort()).GetByteCount();
         if (bytesSent > 0)
         {
            ret += bytesSent;
            LogTime(MUSCLE_LOG_TRACE, "Sent " INT32_FORMAT_SPEC "/" INT32_FORMAT_SPEC " bytes of Discovery pong to %s\n", bytesSent, bufRef()->GetNumBytes(), replyTarget.ToString()());
         }
      }
      (void) _outputData.RemoveFirst();
   }
   return ret;
}

};  // end namespace zg

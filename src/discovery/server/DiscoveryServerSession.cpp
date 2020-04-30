#include "reflector/StorageReflectConstants.h"
#include "system/SystemInfo.h"
#include "system/DetectNetworkConfigChangesSession.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "zg/discovery/server/DiscoveryServerSession.h"
#include "zg/discovery/server/IDiscoveryServerSessionController.h"
#include "zg/private/PZGHeartbeatSettings.h"  // for IsNetworkInterfaceAcceptable()

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

DiscoveryServerSession :: DiscoveryServerSession(bool useWatcher, IDiscoveryServerSessionController & master, uint16 discoveryPort)
   : _useWatcher(useWatcher)
   , _discoveryPort(discoveryPort)
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
   if (_useWatcher)
   {
      _watchInterfacesSession.SetRef(newnothrow DiscoveryDetectNetworkConfigChangesSession(this));
      if (_watchInterfacesSession() == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
      if (AddNewSession(_watchInterfacesSession) != B_NO_ERROR)
      {
         LogTime(MUSCLE_LOG_ERROR, "DiscoveryServerSession:  Error adding interfaces-change-notifier session!\n");
         _watchInterfacesSession.Reset();
      }
   }

   _receiveBuffer = GetByteBufferFromPool(2048);
   return (_receiveBuffer()) ? AbstractReflectSession::AttachedToServer() : B_ERROR;
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

static IPAddress GetDiscoveryMulticastKey()
{
   static const IPAddress _defaultDiscoveryAddress = Inet_AtoN("::baba:babe:bebe:beeb");  // Default core multicast address, not including any multicast prefix
   return _defaultDiscoveryAddress;
}

static IPAddress MakeMulticastAddressLinkLocal(const IPAddress & ip, bool nodeLocal)
{
   IPAddress a = ip;
   a.SetHighBits((ip.GetHighBits()&~(((uint64)0xFFFF)<<48))|(((uint64)(nodeLocal?0xFF11:0xFF12))<<48));
   return a;
}

static status_t MakeIPv6MulticastAddresses(const IPAddress & ip, const Queue<NetworkInterfaceInfo> & niis, Queue<IPAddress> & retAddresses)
{
   if (niis.IsEmpty())
   {
      // If we have no multicast-able interfaces, then we'll fall back to node-local multicast
      IPAddress a = MakeMulticastAddressLinkLocal(ip, true);
      a.SetInterfaceIndex(0);  // paranoia?
      return retAddresses.AddTail(a);  // guaranteed not to fail
   }
   else
   {
      status_t ret;
      if (retAddresses.EnsureSize(retAddresses.GetNumItems()+niis.GetNumItems()).IsError(ret)) return ret;
      for (uint32 i=0; i<niis.GetNumItems(); i++)
      {
         IPAddress a = MakeMulticastAddressLinkLocal(ip, false);
         a.SetInterfaceIndex(niis[i].GetLocalAddress().GetInterfaceIndex());
         (void) retAddresses.AddTail(a);  // guaranteed not to fail
      }
   }
   return B_NO_ERROR;
}

static status_t GetDiscoveryMulticastAddresses(Queue<IPAddressAndPort> & retIAPs, uint16 discoPort)
{
   Queue<NetworkInterfaceInfo> niis;
   GNIIFlags flags(GNII_FLAG_INCLUDE_ENABLED_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES);
   status_t ret = muscle::GetNetworkInterfaceInfos(niis, flags);
   if (ret.IsError()) return ret;

   for (int32 i=niis.GetNumItems()-1; i>=0; i--) if (zg_private::IsNetworkInterfaceAcceptable(niis[i]) == false) (void) niis.RemoveItemAt(i);

   Queue<IPAddress> q;
   if (MakeIPv6MulticastAddresses(GetDiscoveryMulticastKey(), niis, q).IsError(ret)) return ret;
   for (uint32 i=0; i<q.GetNumItems(); i++) if (retIAPs.AddTail(IPAddressAndPort(q[i], discoPort)).IsError(ret)) return ret;
   return ret;
}


ConstSocketRef DiscoveryServerSession :: CreateDefaultSocket()
{
   uint32 numAddedGroups = 0;
   ConstSocketRef udpSocket = CreateUDPSocket();
   Queue<IPAddressAndPort> mcastIAPs;
   status_t ret;

   if ((udpSocket())
    && (GetDiscoveryMulticastAddresses(mcastIAPs, _discoveryPort).IsOK(ret))
    && (mcastIAPs.HasItems())
    && (BindUDPSocket(udpSocket, mcastIAPs.Head().GetPort(), NULL, invalidIP, true).IsOK()))
   {
      (void) SetSocketBlockingEnabled(udpSocket, false);
      for (uint32 i=0; i<mcastIAPs.GetNumItems(); i++) 
      {
         if (AddSocketToMulticastGroup(udpSocket, mcastIAPs[i].GetIPAddress()) == B_NO_ERROR) numAddedGroups++;
         else 
         {
            LogTime(MUSCLE_LOG_ERROR, "DiscoveryServerSession::CreateDefaultSocket:  Unable to add socket to multicast group [%s]!\n", Inet_NtoA(mcastIAPs[i].GetIPAddress())());
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

int32 DiscoveryServerSession :: DoInput(AbstractGatewayMessageReceiver & /*receiver*/, uint32 maxBytes)
{
   const ConstSocketRef & s = GetSessionReadSelectSocket();
   if ((s() == NULL)||(_master == NULL)) return -1;

   const uint64 now = GetRunTime64();
   uint32 ret = 0;
   while(ret < maxBytes)
   {
      IPAddress sourceIP;
      uint16 sourcePort;
      const int32 bytesRead = ReceiveDataUDP(s, _receiveBuffer()->GetBuffer(), _receiveBuffer()->GetNumBytes(), false, &sourceIP, &sourcePort);
      if (bytesRead > 0)
      {
         const IPAddressAndPort iap(sourceIP, sourcePort);
         printf("Received " INT32_FORMAT_SPEC " bytes of discovery query data from %s\n", bytesRead, iap.ToString()());

         MessageRef msg = GetMessageFromPool();
         if ((msg())&&(msg()->Unflatten(_receiveBuffer()->GetBuffer(), bytesRead) == B_NO_ERROR))
         {
            //printf("Query Message is: "); msg()->PrintToStream();

            const uint64 pongDelayMicros = _master->HandleDiscoveryPing(msg, iap);
            if (pongDelayMicros != MUSCLE_TIME_NEVER)
            {
               (void) _queuedOutputData.Put(iap, UDPReply(now+pongDelayMicros, msg));
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

int32 DiscoveryServerSession :: DoOutput(uint32 maxBytes)
{
   const ConstSocketRef & s = GetSessionWriteSelectSocket();
   if (s() == NULL) return -1;

   uint32 ret = 0;
   while((_outputData.HasItems())&&(ret < maxBytes))
   {
      const UDPReply & next = *_outputData.GetFirstValue();
      ByteBufferRef bufRef = GetByteBufferFromPool(next.GetData()()->FlattenedSize());
      if (bufRef())
      {
         const IPAddressAndPort & replyTarget = *_outputData.GetFirstKey();
         next.GetData()()->Flatten(bufRef()->GetBuffer());
         const int32 bytesSent = SendDataUDP(s, bufRef()->GetBuffer(), bufRef()->GetNumBytes(), false, replyTarget.GetIPAddress(), replyTarget.GetPort());
         if (bytesSent > 0) 
         {
            ret += bytesSent;
            printf("Sent " INT32_FORMAT_SPEC "/" INT32_FORMAT_SPEC " bytes of Discovery pong to %s\n", bytesSent, bufRef()->GetNumBytes(), replyTarget.ToString()());
         }
      }
      _outputData.RemoveFirst();
   }
   return ret;
}

};  // end namespace zg

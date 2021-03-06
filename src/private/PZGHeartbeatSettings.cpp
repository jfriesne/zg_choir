#include "dataio/SimulatedMulticastDataIO.h"
#include "dataio/UDPSocketDataIO.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"
#include "zg/private/PZGHeartbeatSettings.h"
#include "zg/ZGConstants.h"
#include "zlib/ZLibUtilityFunctions.h"

namespace zg_private
{

PZGHeartbeatSettings :: PZGHeartbeatSettings(const ZGPeerSettings & peerSettings, const ZGPeerID & localPeerID, uint16 dataTCPPort) 
   : zg::ZGPeerSettings(peerSettings)
   , _systemKey(GetSignature().HashCode64() + GetSystemName().HashCode64())
   , _localPeerID(localPeerID)
   , _dataTCPPort(dataTCPPort)
   , _dataUDPPort(PER_SYSTEM_PORT_DATA)    // hard-coded, for now (maybe configurable later on?)
   , _hbUDPPort(PER_SYSTEM_PORT_HEARTBEAT) // hard-coded, for now (maybe configurable later on?)
   , _birthdate(GetRunTime64()) 
   , _peerAttributesByteBuffer(GetPeerAttributes()() ? DeflateByteBuffer(GetPeerAttributes()()->FlattenToByteBuffer(),9) : ByteBufferRef())
{
   // Catch it if the user starts to get greedy.  Better to find out now than in more subtle ways later!
   if ((_peerAttributesByteBuffer())&&(_peerAttributesByteBuffer()->GetNumBytes() > 65535))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "PZGHeartbeatSettings:  Peer Attributes buffer is too large at " UINT32_FORMAT_SPEC " bytes long!  It should be less than 65536 bytes after zlib-compression (and ideally less than ~800 bytes!)\n", _peerAttributesByteBuffer()->GetNumBytes());
      MCRASH("PZGHeartbeatSettings:  Peer Attributes buffer is too large");
   }
}

static IPAddress GetMulticastAddressForSystemAndPort(const String & signature, const String & systemName, uint16 udpPort)
{
   IPAddress ip = Inet_AtoN("0000:0000:0001::");
   static const uint64 salt = 531763157;  // arbitrary constant
   ip.SetLowBits(ip.GetLowBits() + salt + signature.HashCode64() + systemName.HashCode64() + udpPort);
   ip.SetHighBits((ip.GetHighBits()&~(((uint64)0xFFFF)<<48))|(((uint64)(0xFF02))<<48)); // Make the address prefix link-local (ff02::)
   return ip;
}

// So we can sort our list of network interfaces by interface name.  That makes it easier to scan visually.
class CompareNetworkInterfacesFunctor
{
public:
   int Compare(const NetworkInterfaceInfo & nii1, const NetworkInterfaceInfo & nii2, void *) const {return muscleCompare(nii1.GetName(), nii2.GetName());}
};

Queue<NetworkInterfaceInfo> PZGHeartbeatSettings :: GetNetworkInterfaceInfos() const
{
   Queue<NetworkInterfaceInfo> niis;
   GNIIFlags flags(GNII_FLAG_INCLUDE_ENABLED_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES);
   if (IsSystemOnLocalhostOnly() == false) flags.SetBit(GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES);
   (void) muscle::GetNetworkInterfaceInfos(niis, flags);

   for (int32 i=niis.GetNumItems()-1; i>=0; i--) if (IsNetworkInterfaceUsableForMulticast(niis[i]) == false) (void) niis.RemoveItemAt(i);
   niis.Sort(CompareNetworkInterfacesFunctor());
   return niis;
}

// For Wi-Fi we'll use a separate multicast address, just so we can keep the Wi-Fi simulated-multicast-control-packets 
// off of the multicast address used by the real-multicast (non-Wi-Fi) packets.  That will keep the real-multicast packet-parsers
// from complaining about the unexpected traffic every 10 seconds
static IPAddress MungeMulticastAddress(const IPAddress & origMulticastAddress)
{
   IPAddress ret = origMulticastAddress;
   ret.SetLowBits(ret.GetLowBits()^((uint64)-1));
   return ret;
}

static UDPSocketDataIORef CreateMulticastDataIO(const IPAddressAndPort & multicastIAP)
{
   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock())
   {
      // This must be done before adding the socket to any multicast groups, otherwise Windows gets uncooperative
      if (BindUDPSocket(udpSock, multicastIAP.GetPort(), NULL, invalidIP, true).IsOK())
      {
         const uint8 dummyBuf = 0;  // doesn't matter what this is, I just want to make sure I can actually send on this socket
         if (SendDataUDP(udpSock, &dummyBuf, 0, true, multicastIAP.GetIPAddress(), multicastIAP.GetPort()) == 0)
         {
            if (AddSocketToMulticastGroup(udpSock, multicastIAP.GetIPAddress()).IsOK())
            {
               UDPSocketDataIORef ret(newnothrow UDPSocketDataIO(udpSock, false));
               if (ret()) (void) ret()->SetPacketSendDestination(multicastIAP);
                     else MWARN_OUT_OF_MEMORY;
               return ret;
            }
            else LogTime(MUSCLE_LOG_ERROR, "Unable to add UDP socket to multicast address [%s]\n", multicastIAP.GetIPAddress().ToString()());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Unable to send test UDP packet to multicast destination [%s]\n", multicastIAP.ToString()());
      }
      else LogTime(MUSCLE_LOG_ERROR, "Unable to bind multicast socket to UDP port %u!\n", multicastIAP.GetPort());
   }
   else LogTime(MUSCLE_LOG_ERROR, "CreateMulticastDataIO:  CreateUDPSocket() failed!\n");

   return UDPSocketDataIORef();
}

Queue<PacketDataIORef> PZGHeartbeatSettings :: CreateMulticastDataIOs(bool isForHeartbeats, bool includeWiFi) const
{
   const char * dataDesc = isForHeartbeats ? "heartbeats" : "data";
   Queue<PacketDataIORef> ret;
   const uint16 udpPort = isForHeartbeats ? _hbUDPPort : _dataUDPPort;
   const IPAddress multicastAddress = GetMulticastAddressForSystemAndPort(GetSignature(), GetSystemName(), udpPort);
   Queue<NetworkInterfaceInfo> niis = GetNetworkInterfaceInfos();
   Queue<int> iidxQ;

   /** This enumeration defines some different approaches that ZG can use to handle multicast packets on a given network interface */
   enum {
      MULTICAST_MODE_AUTO = 0,  ///< Default mode -- use "real multicast" for wired network interfaces, and "simulated multicast" for Wi-Fi
      MULTICAST_MODE_STANDARD,  ///< Use "real multicast packets" on this network interface
      MULTICAST_MODE_SIMULATED, ///< Use "simulated multicast" on this network interface
      MULTICAST_MODE_DISABLED,  ///< Don't use this network interface at all
      NUM_MULTICAST_MODES       ///< Guard value
   };

   for (int32 i=niis.GetNumItems()-1; i>=0; i--)
   {
      const NetworkInterfaceInfo & nii = niis[i];

      IPAddress nextMulticastAddress = multicastAddress;
      int iidx = nii.GetLocalAddress().GetInterfaceIndex();
      if ((iidx > 0)&&(iidxQ.Contains(iidx) == false))
      {
         nextMulticastAddress.SetInterfaceIndex(iidx);

         int modeForThisNIC;
         switch(GetMulticastBehavior())
         {
            case ZG_MULTICAST_BEHAVIOR_STANDARD_ONLY:  modeForThisNIC = MULTICAST_MODE_STANDARD;  break;
            case ZG_MULTICAST_BEHAVIOR_SIMULATED_ONLY: modeForThisNIC = MULTICAST_MODE_SIMULATED; break;
            case ZG_MULTICAST_BEHAVIOR_AUTO: default:  modeForThisNIC = MULTICAST_MODE_AUTO;      break;
         }

         const char * ifTypeDesc = "other";

         // Decide which multicast mode to use   
         if (nii.GetHardwareType() == NETWORK_INTERFACE_HARDWARE_TYPE_WIFI)
         {
            if (includeWiFi)
            {
               ifTypeDesc = "WiFi";

               // Prefer simulated-multicast for Wi-Fi (real-multicast over Wi-Fi performs badly)
               if (modeForThisNIC == MULTICAST_MODE_AUTO) modeForThisNIC = MULTICAST_MODE_SIMULATED;
            }
            else modeForThisNIC = MULTICAST_MODE_DISABLED;
         }
         else
         {
            ifTypeDesc = "wired";
            if (modeForThisNIC == MULTICAST_MODE_AUTO) modeForThisNIC = MULTICAST_MODE_STANDARD;
         }

         switch(modeForThisNIC)
         {
            case MULTICAST_MODE_SIMULATED:
            {
               SimulatedMulticastDataIORef wifiIO(newnothrow SimulatedMulticastDataIO(IPAddressAndPort(MungeMulticastAddress(nextMulticastAddress), udpPort)));
               if ((wifiIO())&&(ret.AddTail(wifiIO).IsOK())) 
               {
                  LogTime(MUSCLE_LOG_DEBUG, "Using SimulatedMulticastDataIO for %s on %s interface [%s]\n", dataDesc, ifTypeDesc, nii.ToString()());
                  (void) iidxQ.AddTail(iidx);
               }
               else MWARN_OUT_OF_MEMORY;
            }
            break;

            case MULTICAST_MODE_STANDARD:
            {
               UDPSocketDataIORef wiredIO = CreateMulticastDataIO(IPAddressAndPort(nextMulticastAddress, isForHeartbeats ? _hbUDPPort : _dataUDPPort));
               if ((wiredIO())&&(ret.AddTail(wiredIO).IsOK()))
               {
                  LogTime(MUSCLE_LOG_DEBUG, "Using UDPSocketDataIO for %s on %s interface [%s]\n", dataDesc, ifTypeDesc, nii.ToString()());
                  (void) iidxQ.AddTail(iidx);
               }
               else LogTime(MUSCLE_LOG_ERROR, "Couldn't create multicast data IO %s on %s interface [%s]\n", dataDesc, ifTypeDesc, nii.ToString()());
            }
            break;

            default:
               // do nothing
            break;
         }
      }
   }
   return ret;
}

};  // end namespace zg_private

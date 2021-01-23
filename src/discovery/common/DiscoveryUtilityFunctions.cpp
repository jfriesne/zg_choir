#include "system/DetectNetworkConfigChangesSession.h"
#include "util/NetworkUtilityFunctions.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"

namespace zg {

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

static status_t MakeIPv6MulticastAddresses(const IPAddress & ip, const Queue<NetworkInterfaceInfo> & niis, Hashtable<IPAddress, bool> & retAddresses)
{
   if (niis.IsEmpty())
   {
      // If we have no multicast-able interfaces, then we'll fall back to node-local multicast
      IPAddress a = MakeMulticastAddressLinkLocal(ip, true);
      a.SetInterfaceIndex(0);  // paranoia?
      return retAddresses.Put(a, false);  // guaranteed not to fail
   }
   else
   {
      status_t ret;
      if (retAddresses.EnsureSize(retAddresses.GetNumItems()+niis.GetNumItems()).IsError(ret)) return ret;
      for (uint32 i=0; i<niis.GetNumItems(); i++)
      {
         const NetworkInterfaceInfo & nii = niis[i];
         IPAddress a = MakeMulticastAddressLinkLocal(ip, false);
         a.SetInterfaceIndex(nii.GetLocalAddress().GetInterfaceIndex());
         (void) retAddresses.Put(a, (nii.GetHardwareType()==NETWORK_INTERFACE_HARDWARE_TYPE_WIFI));  // guaranteed not to fail
      }
   }
   return B_NO_ERROR;
}

static status_t GetMulticastAddresses(Hashtable<IPAddressAndPort, bool> & retIAPs, uint16 port, const IPAddress & baseKey)
{
   Queue<NetworkInterfaceInfo> niis;
   GNIIFlags flags(GNII_FLAG_INCLUDE_ENABLED_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES);
   status_t ret = muscle::GetNetworkInterfaceInfos(niis, flags);
   if (ret.IsError()) return ret;

   for (int32 i=niis.GetNumItems()-1; i>=0; i--) if (IsNetworkInterfaceUsableForMulticast(niis[i]) == false) (void) niis.RemoveItemAt(i);

   Hashtable<IPAddress, bool> q;
   if (MakeIPv6MulticastAddresses(baseKey, niis, q).IsError(ret)) return ret;
   for (HashtableIterator<IPAddress, bool> iter(q); iter.HasData(); iter++) if (retIAPs.Put(IPAddressAndPort(iter.GetKey(), port), iter.GetValue()).IsError(ret)) return ret;
   return ret;
}

status_t GetDiscoveryMulticastAddresses(Hashtable<IPAddressAndPort, bool> & retIAPs, uint16 discoPort)
{
   return GetMulticastAddresses(retIAPs, discoPort, GetDiscoveryMulticastKey());
}

static IPAddressAndPort GetTransceiverMulticastKey(const String & transmissionKey)
{
   const uint64 tHash = transmissionKey.HashCode64(); 
   IPAddress ip = Inet_AtoN("::a1:a2:a3:a4");  // base core multicast address, not including any multicast prefix
   ip.SetLowBits(ip.GetLowBits()+tHash);
   return IPAddressAndPort(ip, ((uint16)(tHash%30000))+30000);  // I guess?
}

status_t GetTransceiverMulticastAddresses(Hashtable<IPAddressAndPort, bool> & retIAPs, const String & transmissionKey)
{
   const IPAddressAndPort iap = GetTransceiverMulticastKey(transmissionKey);
   return GetMulticastAddresses(retIAPs, iap.GetPort(), iap.GetIPAddress());
}

// Some network interfaces we just shouldn't try to use!
bool IsNetworkInterfaceUsableForMulticast(const NetworkInterfaceInfo & nii)
{  
#ifdef __APPLE__
   if (nii.GetName().StartsWith("utun")) return false;
   if (nii.GetName().StartsWith("llw"))  return false;
#else 
   (void) nii;  // avoid compiler warning
#endif
   
   return nii.GetLocalAddress().IsSelfAssigned();  // fe80::blah addresses (or similar) only, please!
}

};  // end namespace zg

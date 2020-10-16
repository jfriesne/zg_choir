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

status_t GetDiscoveryMulticastAddresses(Queue<IPAddressAndPort> & retIAPs, uint16 discoPort)
{
   Queue<NetworkInterfaceInfo> niis;
   GNIIFlags flags(GNII_FLAG_INCLUDE_ENABLED_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES);
   status_t ret = muscle::GetNetworkInterfaceInfos(niis, flags);
   if (ret.IsError()) return ret;

   for (int32 i=niis.GetNumItems()-1; i>=0; i--) if (IsNetworkInterfaceUsableForDiscovery(niis[i]) == false) (void) niis.RemoveItemAt(i);

   Queue<IPAddress> q;
   if (MakeIPv6MulticastAddresses(GetDiscoveryMulticastKey(), niis, q).IsError(ret)) return ret;
   for (uint32 i=0; i<q.GetNumItems(); i++) if (retIAPs.AddTail(IPAddressAndPort(q[i], discoPort)).IsError(ret)) return ret;
   return ret;
}

// Some network interfaces we just shouldn't try to use!
bool IsNetworkInterfaceUsableForDiscovery(const NetworkInterfaceInfo & nii)
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

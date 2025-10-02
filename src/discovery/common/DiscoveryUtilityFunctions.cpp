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
      MRETURN_ON_ERROR(retAddresses.EnsureSize(retAddresses.GetNumItems()+niis.GetNumItems()));
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

static status_t GetMulticastAddresses(Hashtable<IPAddressAndPort, bool> & retIAPs, uint16 port, const IPAddress & baseKey, const StringMatcher * optNicNameFilter)
{
   Queue<NetworkInterfaceInfo> niis;
   MRETURN_ON_ERROR(muscle::GetNetworkInterfaceInfos(niis, GNIIFlags(GNII_FLAG_INCLUDE_ENABLED_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES)));

   for (int32 i=niis.GetLastValidIndex(); i>=0; i--)
   {
      const NetworkInterfaceInfo & nii = niis[i];
      if ((nii.IsCopperDetected() == false)||(IsNetworkInterfaceUsableForMulticast(nii) == false)||((optNicNameFilter)&&(optNicNameFilter->Match(nii.GetName()()) == false))) (void) niis.RemoveItemAt(i);
   }

//for(uint32 i=0; i<niis.GetNumItems(); i++) printf(" --> %s\n", niis[i].ToString()());

   Hashtable<IPAddress, bool> q;
   MRETURN_ON_ERROR(MakeIPv6MulticastAddresses(baseKey, niis, q));
   for (HashtableIterator<IPAddress, bool> iter(q); iter.HasData(); iter++)
   {
      const IPAddressAndPort iap(iter.GetKey(), port);
      const bool isWiFi = iter.GetValue();
      MRETURN_ON_ERROR(retIAPs.Put(iap, isWiFi));
      LogTime(MUSCLE_LOG_TRACE, "GetMulticastAddresses:  Using address [%s] isWiFi=%i for baseKey=%s\n", iap.ToString()(), isWiFi, baseKey.ToString()());
   }
   return B_NO_ERROR;
}

status_t GetDiscoveryMulticastAddresses(Hashtable<IPAddressAndPort, bool> & retIAPs, uint16 discoPort)
{
   return GetMulticastAddresses(retIAPs, discoPort, GetDiscoveryMulticastKey(), NULL);
}

static IPAddressAndPort GetTransceiverMulticastKey(const String & transmissionKey)
{
   const uint64 tHash = transmissionKey.HashCode64();
   IPAddress ip = Inet_AtoN("::a1:a2:a3:a4");  // base core multicast address, not including any multicast prefix
   ip.SetLowBits(ip.GetLowBits()+tHash);
   return IPAddressAndPort(ip, ((uint16)(tHash%30000))+20000);  // I guess?
}

status_t GetTransceiverMulticastAddresses(Hashtable<IPAddressAndPort, bool> & retIAPs, const String & transmissionKey, const StringMatcher * optNicNameFilter)
{
   const IPAddressAndPort iap = GetTransceiverMulticastKey(transmissionKey);
   return GetMulticastAddresses(retIAPs, iap.GetPort(), iap.GetIPAddress(), optNicNameFilter);
}

// Some network interfaces we just shouldn't try to use!
bool IsNetworkInterfaceUsableForMulticast(const NetworkInterfaceInfo & nii)
{
#ifdef __APPLE__
   if (nii.GetName().StartsWith("utun")) return false;
   if (nii.GetName().StartsWith("llw"))  return false;
   if (nii.GetName().StartsWith("awdl")) return false;
#else
   (void) nii;  // avoid compiler warning
#endif

   return nii.GetLocalAddress().IsSelfAssigned();  // fe80::blah addresses (or similar) only, please!
}

};  // end namespace zg

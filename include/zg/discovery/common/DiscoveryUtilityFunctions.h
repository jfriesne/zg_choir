#ifndef DiscoveryConstants_h
#define DiscoveryConstants_h

#include "util/IPAddress.h"
#include "util/NetworkInterfaceInfo.h"
#include "zg/ZGNameSpace.h"

namespace zg {

enum {DEFAULT_ZG_DISCOVERY_PORT = 25672};  /**< Arbitrary default port number to use for ZG's discovery traffic */

#define ZG_DISCOVERY_NAME_SIGNATURE  "sig" /**< Name of string field containing the System's program-signature */
#define ZG_DISCOVERY_NAME_SYSTEMNAME "sn"  /**< Name of string field containing the System Name */
#define ZG_DISCOVERY_NAME_PEERID     "pid" /**< Name of field containing a flattened ZGPeerID object */
#define ZG_DISCOVERY_NAME_FILTER     "flt" /**< Name of Message sub-field containing archived QueryFilter object */
#define ZG_DISCOVERY_NAME_TAG        "tag" /**< Name of Misc field supplied in ping message, copied to pong verbatim */
#define ZG_DISCOVERY_NAME_TIMESYNCPORT "tsp" /**< uint16 containing UDP port number where the ZGPeer is accepting UDP packets for time-synchronization purposes */

#define ZG_DISCOVERY_NAME_PEERINFO   "inf" /**< Name of Message field containing per-Peer information Messages */
#define ZG_DISCOVERY_NAME_SOURCE     "src" /**< Name of String field containing the source IPAddressAndPort */

/** Returns the set of multicast groups currently recommended to send/receive discovery packets on.
  * @param retIAPs on successful return, this will return one IPAddressAndPort per local network device to use.
  * @param discoPort Which UDP port to use for discovery.  Defaults to DEFAULT_ZG_DISCOVERY_PORT.
  * @returns B_NO_ERROR on success, or some other error code on failure.
  */
status_t GetDiscoveryMulticastAddresses(Queue<IPAddressAndPort> & retIAPs, uint16 discoPort = DEFAULT_ZG_DISCOVERY_PORT);

/** Returns true iff (nii) is a Network interface we should actually try to use, or false if we should avoid it (because it's e.g. known to be a special-purpose thing) */
bool IsNetworkInterfaceUsableForDiscovery(const NetworkInterfaceInfo & nii);

};  // end namespace zg

#endif

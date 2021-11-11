#ifndef PZGHeartbeatSettings_h
#define PZGHeartbeatSettings_h

#include "dataio/DataIO.h"
#include "dataio/PacketDataIO.h"
#include "zg/INetworkInterfaceFilter.h"
#include "zg/ZGPeerID.h"
#include "zg/ZGPeerSettings.h"
#include "zg/private/PZGNameSpace.h"
#include "util/NetworkUtilityFunctions.h"  // for NetworkInterfaceInfo
#include "util/String.h"
#include "util/RefCount.h"

namespace zg_private
{

/** This immutable class holds various read-only settings that will be used by the heartbeat thread.
  * By grouping them together into a single object, we can pass them from one object to the
  * next more easily than if we had to pass every value separately, and also we make sure that
  * they can't be changed in a way that would be non-thread-safe.
  */
class PZGHeartbeatSettings : public zg::ZGPeerSettings, public RefCountable
{
public:
   PZGHeartbeatSettings(const ZGPeerSettings & peerSettings, const ZGPeerID & localPeerID, uint16 dataTCPPort);

   uint64 GetSystemKey()             const {return _systemKey;}   // (_systemName.HashCode64()+_signature.HashCode64()), precomputed for convenience
   const ZGPeerID & GetLocalPeerID() const {return _localPeerID;} // unique 128-bit ID of our local peer
   uint16 GetDataTCPPort()           const {return _dataTCPPort;} // port that our peer will listen for incoming TCP data connections on
   uint64 GetBirthdateMicros()       const {return _birthdate;}   // the moment at which this state was created (using the local GetRunTime64() clock)

   /** Pre-flattened byte-buffer from our GetPeerAttributesMessage(), for convenience */
   const ConstByteBufferRef & GetPeerAttributesByteBuffer() const {return _peerAttributesByteBuffer;}

   /** Convenience method:  Creates and returns a set of Multicast UDP sockets suitable for use with either heartbeat or data traffic.
     * @param isForHeartbeats If true, this socket is for use carrying heartbeats traffic; if false, it's intended to be used for data-payload traffic
     * @param optNetworkInterfaceFilter if non-NULL, we'll call IsOkayToUseNetworkInterface() on this object to decide whether or not we are allowed to
     *                                  use a particular network interface.
     * @returns A list of DataIORefs.  On failure, the list will be empty.
     */
   Queue<PacketDataIORef> CreateMulticastDataIOs(bool isForHeartbeats, const INetworkInterfaceFilter * optNetworkInterfaceFilter) const;

   /** Returns a list of network interfaces that are okay for us to use if we want to */
   Queue<NetworkInterfaceInfo> GetNetworkInterfaceInfos() const;

private:
   const uint64 _systemKey;
   const ZGPeerID _localPeerID; // unique 128-bit ID of our local peer
   const uint16 _dataTCPPort;   // port that our peer will listen for incoming TCP data connections on
   const uint16 _dataUDPPort;   // port that we will be listening for multicast data-payload UDP traffic on
   const uint16 _hbUDPPort;     // port that we will be listening for multicast heartbeat UDP traffic on
   const uint64 _birthdate;     // timestamp of the moment at which this state-object was created (as returned by GetRunTime64())

   const ConstByteBufferRef _peerAttributesByteBuffer;
};
DECLARE_REFTYPES(PZGHeartbeatSettings);

};  // end namespace zg_private

#endif

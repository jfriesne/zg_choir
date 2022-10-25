#ifndef PZGHeartbeatSourceState_h
#define PZGHeartbeatSourceState_h

#include "zg/private/PZGRoundTripTimeAverager.h"
#include "zg/INetworkTimeProvider.h"

namespace zg_private
{

/** This class holds all of the state associated with a particular source of heartbeats.
  * The heartbeat thread will create one of these for each (ZGPeerID/IPAddressAndPort)
  * tuple it is currently receiving heartbeats from.
  *
  * This class computes a running average of heartbeat packet round-trip times that we
  * can use to synchronize our network-clock with the network-clock running on the remote
  * peer, if/when that remote peer becomes the senior peer of the system.
  */
class PZGHeartbeatSourceState : public RefCountable
{
public:
   /** Constructor
     * @param maxMeasurements The maximum number of measurements we should factor in to our running average.
     */
   PZGHeartbeatSourceState(uint32 maxMeasurements);

   /** Adds a new measurement into the running average.
     * @param multicastAddress The multicast address we originally sent our heartbeat packet out on
     * @param newMeasurementMicros The new round-trip-time measurement across (multicastAddress)
     * @param localNowMicros The current local clock time (as given by GetRunTime64())
     * @returns B_NO_ERROR on success, or an error code on failure
     */
   status_t AddMeasurement(const IPAddressAndPort & multicastAddress, uint64 newMeasurementMicros, uint64 localNowMicros);

   /** Returns the current running average over the last N measurements for the given interface, or 0 if we have no measurements right now. */
   uint64 GetAverageValueIgnoringOutliers(const IPAddressAndPort & multicastAddr) const
   {
      const PZGRoundTripTimeAveragerRef * rtt = _rttAveragers.Get(multicastAddr);
      return rtt ? rtt->GetItemPointer()->GetAverageValueIgnoringOutliers() : 0;
   }

   /** Returns the current running average over the last N measurements for our currently-preferred interface.
     * or 0 if we have no measurements available right now.
     * @param mustHaveMeasurementsAfterThisTime Any PZGRoundTripTimeAveragers that haven't received measurements
     *                                          since this time (local clock, in microseconds) will not be used.
     */
   uint64 GetPreferredAverageValue(uint64 mustHaveMeasurementsAfterThisTime);

   /** Set the current heartbeat packet that we have from this source, and when it should expire */
   void SetHeartbeatPacket(const PZGHeartbeatPacketWithMetaDataRef & hbPacket, uint64 localExpirationTimeMicros)
   {
      _hbPacket = hbPacket;
      _localExpirationTimeMicros = localExpirationTimeMicros;
   }
   const PZGHeartbeatPacketWithMetaDataRef & GetHeartbeatPacket() const {return _hbPacket;}

   // Time at which this source will be marked as offline if we don't get any further heartbeats from it
   uint64 GetLocalExpirationTimeMicros() const {return _localExpirationTimeMicros;}

   // Returns our current state as a human-readable string, for debugging
   String ToString(const INetworkTimeProvider & ntp) const;

   /** Discards the PZGRoundTripTimeAverager (if any) associated with the specified multicast address.
     * @param multicastAddress the multicast address (ff12::blah) that the averager is associated with
     * @returns B_NO_ERROR if the averagers was found and discarded, or an error code if it wasn't found.
     */
   status_t DiscardRoundTripTimeAverager(const IPAddressAndPort & multicastAddress) {return _rttAveragers.Remove(multicastAddress);}

private:
   const uint32 _maxMeasurements;

   Hashtable<IPAddressAndPort, PZGRoundTripTimeAveragerRef> _rttAveragers;
   PZGHeartbeatPacketWithMetaDataRef _hbPacket;
   uint64 _localExpirationTimeMicros;
};
DECLARE_REFTYPES(PZGHeartbeatSourceState);

};  // end namespace zg_private

#endif

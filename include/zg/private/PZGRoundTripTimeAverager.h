#ifndef PZGRoundTripTimeAverager_h
#define PZGRoundTripTimeAverager_h

#include "zg/clocksync/ZGTimeAverager.h"
#include "zg/private/PZGHeartbeatPacket.h"

namespace zg_private
{

/** This is a class that computes a running average the last N heartbeat-round-trip times,
  * to avoid too much jitter in the network-time computations
  */
class PZGRoundTripTimeAverager : public ZGTimeAverager
{
public:
   /** Constructor
     * @param maxMeasurements The maximum number of measurements we should factor in to our running average.
     */
   PZGRoundTripTimeAverager(uint32 maxMeasurements) : ZGTimeAverager(maxMeasurements), _localExpirationTimeMicros(0) {/* empty */}

   /** Destructor */
   virtual ~PZGRoundTripTimeAverager() {/* empty */}

   /** Set the current heartbeat packet that we have from this source, and when it should expire */
   void SetHeartbeatPacket(const PZGHeartbeatPacketWithMetaDataRef & hbPacket, uint64 localExpirationTimeMicros)
   {
      _hbPacket = hbPacket;
      _localExpirationTimeMicros = localExpirationTimeMicros;
   }

   /** Returns a reference to our current heartbeat-packet */
   const PZGHeartbeatPacketWithMetaDataRef & GetHeartbeatPacket() const {return _hbPacket;}

   /** Returns the time at which this source will be marked as offline if we don't receive any further heartbeats from it */
   uint64 GetLocalExpirationTimeMicros() const {return _localExpirationTimeMicros;}

private:
   PZGHeartbeatPacketWithMetaDataRef _hbPacket;
   uint64 _localExpirationTimeMicros;
};
DECLARE_REFTYPES(PZGRoundTripTimeAverager);

};  // end namespace zg_private

#endif

#ifndef PZGRoundTripTimeAverager_h
#define PZGRoundTripTimeAverager_h

#include "zg/private/PZGNameSpace.h"
#include "zg/private/PZGHeartbeatPacket.h"
#include "util/Queue.h"
#include "util/RefCount.h"

namespace zg_private
{

/** This is a class that computes a running average the last N round-trip times,
  * to avoid too much jitter in the network-time computations
  */
class PZGRoundTripTimeAverager : public RefCountable
{
public:
   /** Constructor
     * @param maxMeasurements The maximum number of measurements we should factor in to our running average.
     */
   PZGRoundTripTimeAverager(uint32 maxMeasurements) : _maxMeasurements(maxMeasurements), _totalMicros(0), _lastMeasurementTime(0), _localExpirationTimeMicros(0), _cachedAverageWithoutOutliers(-1) {/* empty */}

   /** Destructor */
   virtual ~PZGRoundTripTimeAverager() {/* empty */}

   /** Adds a new measurement into the running average.
     * @param newMeasurementMicros The new round-trip-time measurement
     * @param now The current time, e.g. as returned by GetRunTime64()
     * @returns B_NO_ERROR on success, or B_ERROR on failure
     */
   status_t AddMeasurement(uint64 newMeasurementMicros, uint64 now)
   {
      while(_measurements.GetNumItems() >= _maxMeasurements) RemoveOldMeasurement();
      if (_measurements.AddTail(newMeasurementMicros) != B_NO_ERROR) return B_ERROR;
      _totalMicros                 += newMeasurementMicros;
      _lastMeasurementTime          = now;
      _cachedAverageWithoutOutliers = -1;
      return B_NO_ERROR; 
   }

   /** Returns the current running average over the last N measurements, or 0 if we have no measurements right now. */
   uint64 GetRawAverageValue() const
   {
      return _measurements.HasItems() ? (_totalMicros/_measurements.GetNumItems()) : 0;
   }

   /** Returns the average value ignoring any outliers (where "outlier" is defined as any sample
     * value more than a standard deviation away from the value returned by GetRawAverageValue()).
     * Note that this method is more expensive than GetRawAverageValue(), but hopefully not unduly so.
     */
   uint64 GetAverageValueIgnoringOutliers() const
   {
      if (_cachedAverageWithoutOutliers < 0) _cachedAverageWithoutOutliers = GetAverageValueIgnoringOutliersAux();
      return _cachedAverageWithoutOutliers;
   }

   /** Returns the wall-clock time at which we last added a measurement, or 0 if we never added one. */
   uint64 GetLastMeasurementTime() const {return _lastMeasurementTime;}

   /** Clears our set of recorded measurements. */
   void Clear() {_measurements.Clear(); _totalMicros = 0; _cachedAverageWithoutOutliers = -1;}

   /** Returns the current number of measurements we have stored */
   uint32 GetNumMeasurements() const {return _measurements.GetNumItems();}

   /** Set the current heartbeat packet that we have from this source, and when it should expire */
   void SetHeartbeatPacket(const PZGHeartbeatPacketWithMetaDataRef & hbPacket, uint64 localExpirationTimeMicros) 
   {
      _hbPacket = hbPacket; 
      _localExpirationTimeMicros = localExpirationTimeMicros;
   }
   const PZGHeartbeatPacketWithMetaDataRef & GetHeartbeatPacket() const {return _hbPacket;}

   // Time at which this source will be marked as offline if we don't get any further heartbeats from it
   uint64 GetLocalExpirationTimeMicros() const {return _localExpirationTimeMicros;}
 
private:
   uint64 GetAverageValueIgnoringOutliersAux() const;

   void RemoveOldMeasurement()
   {
      uint64 oldMeasurement = 0;  // set to zero to avoid compiler warning
      if (_measurements.RemoveHead(oldMeasurement) == B_NO_ERROR) 
      {
         _totalMicros -= oldMeasurement;
         _cachedAverageWithoutOutliers = -1;
      }
   }

   const uint32 _maxMeasurements;
   Queue<uint64> _measurements;
   uint64 _totalMicros;
   uint64 _lastMeasurementTime;  // GetRunTime64()-clock value of when we last added a measurement

   PZGHeartbeatPacketWithMetaDataRef _hbPacket;
   uint64 _localExpirationTimeMicros;

   mutable int64 _cachedAverageWithoutOutliers;
};
DECLARE_REFTYPES(PZGRoundTripTimeAverager);

};  // end namespace zg_private

#endif

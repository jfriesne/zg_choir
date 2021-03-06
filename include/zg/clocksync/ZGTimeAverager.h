#ifndef ZGTimeAverager_h
#define ZGTimeAverager_h

#include "util/Queue.h"
#include "util/RefCount.h"
#include "zg/ZGNameSpace.h"

namespace zg
{

/** This is a class that computes a running average the last N recorded times
  * to avoid too much jitter in the network-time computations
  */
class ZGTimeAverager : public RefCountable
{
public:
   /** Constructor
     * @param maxMeasurements The maximum number of measurements we should factor in to our running average.
     */
   ZGTimeAverager(uint32 maxMeasurements) : _maxMeasurements(maxMeasurements), _totalMicros(0), _lastMeasurementTime(0), _cachedAverageWithoutOutliers(-1) {/* empty */}

   /** Destructor */
   virtual ~ZGTimeAverager() {/* empty */}

   /** Adds a new measurement into the running average.
     * @param newMeasurementMicros The new round-trip-time measurement
     * @param now The current time, e.g. as returned by GetRunTime64()
     * @returns B_NO_ERROR on success, or an error code on failure
     */
   status_t AddMeasurement(uint64 newMeasurementMicros, uint64 now);

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

private:
   uint64 GetAverageValueIgnoringOutliersAux() const;
   void RemoveOldMeasurement();

   const uint32 _maxMeasurements;
   Queue<uint64> _measurements;
   uint64 _totalMicros;
   uint64 _lastMeasurementTime;  // GetRunTime64()-clock value of when we last added a measurement

   mutable int64 _cachedAverageWithoutOutliers;
};
DECLARE_REFTYPES(ZGTimeAverager);

};  // end namespace zg

#endif

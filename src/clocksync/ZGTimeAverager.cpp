#include <math.h>
#include "zg/clocksync/ZGTimeAverager.h"

namespace zg {

status_t ZGTimeAverager :: AddMeasurement(uint64 newMeasurementMicros, uint64 now)
{
   while(_measurements.GetNumItems() >= _maxMeasurements) RemoveOldMeasurement();
   if (_measurements.AddTail(newMeasurementMicros) != B_NO_ERROR) return B_ERROR;
   _totalMicros                 += newMeasurementMicros;
   _lastMeasurementTime          = now;
   _cachedAverageWithoutOutliers = -1;
   return B_NO_ERROR;
}

uint64 ZGTimeAverager :: GetAverageValueIgnoringOutliersAux() const
{
   if (_measurements.IsEmpty()) return 0;

   const int64 rawAverage = GetRawAverageValue();
   uint64 sumOfDiffs = 0;
   for (uint32 i=0; i<_measurements.GetNumItems(); i++)
   {
      const int64 diff = ((int64)_measurements[i])-rawAverage;
      sumOfDiffs += (diff*diff);
   }

   const double maxDeviations = 1.0;
   const double stdDeviation  = sqrt((double)(sumOfDiffs/_measurements.GetNumItems()));
   const int64 maxDelta       = (uint64) (maxDeviations*stdDeviation);

   uint64 newSum   = 0;
   uint32 newCount = 0;
   for (uint32 i=0; i<_measurements.GetNumItems(); i++)
   {
      const uint64 m = _measurements[i];
      if (muscleAbs(((int64)m)-rawAverage) <= maxDelta)
      {
         newSum += m;
         newCount++;
      }
   }
   return (newCount > 0) ? (newSum/newCount) : 0;
}

void ZGTimeAverager :: RemoveOldMeasurement()
{
   uint64 oldMeasurement = 0;  // set to zero to avoid compiler warning
   if (_measurements.RemoveHead(oldMeasurement) == B_NO_ERROR)
   {
      _totalMicros -= oldMeasurement;
      _cachedAverageWithoutOutliers = -1;
   }
}

};  // end namespace zg

#include "zg/private/PZGHeartbeatSourceState.h"

namespace zg_private
{

PZGHeartbeatSourceState :: PZGHeartbeatSourceState(uint32 maxMeasurements) : _maxMeasurements(maxMeasurements)
{
   // empty
}

status_t PZGHeartbeatSourceState :: AddMeasurement(const IPAddressAndPort & multicastAddr, uint64 newMeasurementMicros, uint64 now)
{
   PZGRoundTripTimeAveragerRef * rtt = _rttAveragers.Get(multicastAddr);
   if (rtt == NULL) 
   {
      PZGRoundTripTimeAveragerRef r(newnothrow PZGRoundTripTimeAverager(_maxMeasurements));
      if (r() == NULL) RETURN_OUT_OF_MEMORY;

      rtt = _rttAveragers.PutAndGet(multicastAddr, r);
      if (rtt == NULL) RETURN_OUT_OF_MEMORY;
   }
   return rtt ? rtt->GetItemPointer()->AddMeasurement(newMeasurementMicros, now) : B_ERROR;
}

uint64 PZGHeartbeatSourceState :: GetPreferredAverageValue(uint64 mustHaveMeasurementsAfterThisTime)
{
   bool isFirstKey = true;
   for (HashtableIterator<IPAddressAndPort, PZGRoundTripTimeAveragerRef> iter(_rttAveragers); iter.HasData(); iter++)
   {
      const PZGRoundTripTimeAverager * rtt = iter.GetValue()(); 
      if (rtt->GetLastMeasurementTime() >= mustHaveMeasurementsAfterThisTime)
      {
         const uint64 ret = rtt->GetAverageValueIgnoringOutliers();
         if (isFirstKey == false)
         {  
            // Let's move the unacceptable averagers to the back; that way we'll stay on our new preference even if they come back
            // That way we avoid "flapping" back and forth if there is a marginal averager for some reason.
            for (HashtableIterator<IPAddressAndPort, PZGRoundTripTimeAveragerRef> iter2(_rttAveragers); iter2.HasData(); iter2++)
            {
               if (iter2.GetKey() == iter.GetKey()) break;
                                               else (void) _rttAveragers.MoveToBack(iter2.GetKey());
            }
         }
         return ret;
      }
      isFirstKey = false;
   } 
   return 0;
}

String PZGHeartbeatSourceState :: ToString(const INetworkTimeProvider & ntp) const
{  
   const PZGHeartbeatPacketWithMetaData * hb = GetHeartbeatPacket()();
   const uint64 localReceivedAt   = hb ? hb->GetLocalReceiveTimeMicros() : 0;
   const uint64 advertisedNetTime = hb ? hb->GetNetworkSendTimeMicros()  : 0;
   const uint64 computedNetTime   = ntp.GetNetworkTime64ForRunTime64(localReceivedAt);
   String ret = String("advertisedNetTime=%1").Arg(advertisedNetTime);
   for (HashtableIterator<IPAddressAndPort, PZGRoundTripTimeAveragerRef> iter(_rttAveragers); iter.HasData(); iter++)
   {
      const uint64 raw    = iter.GetValue()()->GetRawAverageValue();
      const uint64 cooked = iter.GetValue()()->GetAverageValueIgnoringOutliers();
      const  int64 delta  = computedNetTime-advertisedNetTime;
      ret += String(" {addr=[%1] rawRTT=[%2] cookedRTT=[%3] error=[%4]}").Arg(iter.GetKey().ToString()).Arg(GetHumanReadableSignedTimeIntervalString(raw, 1)).Arg(GetHumanReadableSignedTimeIntervalString(cooked, 1)).Arg(GetHumanReadableSignedTimeIntervalString(delta, 1));
   }
   return ret;
}

uint64 PZGRoundTripTimeAverager :: GetAverageValueIgnoringOutliersAux() const
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

};  // end namespace zg_private

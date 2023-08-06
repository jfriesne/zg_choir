#include "zg/private/PZGHeartbeatSourceState.h"

namespace zg_private
{

PZGHeartbeatSourceState :: PZGHeartbeatSourceState(uint32 maxMeasurements)
   : _maxMeasurements(maxMeasurements)
   , _localExpirationTimeMicros(0)
{
   // empty
}

status_t PZGHeartbeatSourceState :: AddMeasurement(const IPAddressAndPort & multicastAddr, uint64 newMeasurementMicros, uint64 now)
{
   PZGRoundTripTimeAveragerRef * rtt = _rttAveragers.Get(multicastAddr);
   if (rtt == NULL)
   {
      PZGRoundTripTimeAveragerRef r(newnothrow PZGRoundTripTimeAverager(_maxMeasurements));
      MRETURN_OOM_ON_NULL(r());

      rtt = _rttAveragers.PutAndGet(multicastAddr, r);
      MRETURN_OOM_ON_NULL(rtt);
   }
   return rtt->GetItemPointer()->AddMeasurement(newMeasurementMicros, now);
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

};  // end namespace zg_private

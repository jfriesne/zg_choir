#ifndef INetworkTimeProvider_h
#define INetworkTimeProvider_h

#include "zg/ZGNameSpace.h"

namespace zg
{

/** This is an abstract interface for an object that can provide us with network-clock-time values */
class INetworkTimeProvider
{
public:
   /** Virtual destructor, just to keep C++ honest */
   virtual ~INetworkTimeProvider() {/* empty */}

   /** Returns the current time according to the network-time-clock, in microseconds.
     * The intent of this clock is to be the same on all peers in the system.  However, this means that it may occasionally
     * change (break monotonicity) in order to synchronize with the other peers in the system.
     * Note that this function will return 0 if we aren't currently fully attached to the system, since before then we don't know the network time.
     */
   MUSCLE_NODISCARD virtual uint64 GetNetworkTime64() const = 0;

   /** Given a network-time-clock-value (ie one using the same time-base as returned by GetNetworkTime64()),
     * returns the approximately-equivalent local-time-clock-value (ie one using the same time-base as returned by GetRunTime64())
     * @param networkTime64TimeStamp a network-clock time, in microseconds
     */
   MUSCLE_NODISCARD virtual uint64 GetRunTime64ForNetworkTime64(uint64 networkTime64TimeStamp) const = 0;

   /** Given a local-time-clock-value (ie one using the same time-base as returned by GetRunTime64()), returns
     * the approximately equivalent network-time-value (ie one using the same time-base as returned by GetNetworkTime64())
     * @param runTime64TimeStamp a local-clock time, in microseconds
     */
   MUSCLE_NODISCARD virtual uint64 GetNetworkTime64ForRunTime64(uint64 runTime64TimeStamp) const = 0;

   /** Returns the number of microseconds that should be added to a GetRunTime64() value to turn it into a GetNetworkTime64() value,
     * or subtracted to do the inverse operation.  Note that this value will vary from one moment to the next!
     */
   MUSCLE_NODISCARD virtual int64 GetToNetworkTimeOffset() const = 0;
};

};  // end namespace zg

#endif

#include "zg/callback/ICallbackSubscriber.h"
#include "zg/callback/ICallbackMechanism.h"

namespace zg {

ICallbackMechanism :: ICallbackMechanism()
{
   // empty
}

ICallbackMechanism :: ~ICallbackMechanism()
{
   // Prevent dangling pointers in any still-registered ICallbackSubscriber objects
   for (HashtableIterator<ICallbackSubscriber *, Void> iter(_registeredSubscribers); iter.HasData(); iter++) iter.GetKey()->SetCallbackMechanism(NULL);
}

void ICallbackMechanism :: DispatchCallbacks()
{
   {
      // Critical section:  grab the set of dirty-subscribers into _scratchSubscribers
      MutexGuard mg(_dirtySubscribersMutex);
      _scratchSubscribers.SwapContents(_dirtySubscribers);
   }

   // Perform requested callbacks
   for (HashtableIterator<ICallbackSubscriber *, uint32> iter(_scratchSubscribers); iter.HasData(); iter++)
   {
      ICallbackSubscriber * sub = iter.GetKey();
      if (_registeredSubscribers.ContainsKey(sub)) sub->DispatchCallbacks(iter.GetValue());  // yes, the if-test is necessary!
   }
   _scratchSubscribers.Clear();
}

void ICallbackMechanism :: RequestCallbackInDispatchThread(ICallbackSubscriber * sub, uint32 eventTypeBits, uint32 clearBits)
{
   if (eventTypeBits != 0)
   {
      bool sendSignal = false;
      {
         // Critical section
         MutexGuard mg(_dirtySubscribersMutex); 
         const bool wasEmpty = _dirtySubscribers.IsEmpty();
         uint32 * subBits = _dirtySubscribers.GetOrPut(sub);
         if (subBits)
         {
            *subBits &= ~clearBits;
            *subBits |= eventTypeBits;
            if (wasEmpty) sendSignal = true;
         }
      }

      // Note that we send the signal AFTER releasing the Mutex
      if (sendSignal) SignalDispatchThread();
   }
}

};  // end zg namespace

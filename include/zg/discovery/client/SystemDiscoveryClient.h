#ifndef SystemDiscoveryClient_h
#define SystemDiscoveryClient_h

#include "message/Message.h"
#include "regex/QueryFilter.h"
#include "util/ICallbackSubscriber.h"
#include "zg/ZGNameSpace.h"

namespace zg {

class IDiscoveryNotificationTarget;
class DiscoveryImplementation;

/** This object is a front-end to the ZG system-discovery logic.
  * Instantiate one of these to get notifications about what ZG systems
  * are currently on-line.
  */
class SystemDiscoveryClient : public ICallbackSubscriber, public RefCountable
{
public:
   /** Constructor.
     * @param mechanism the CallbackMechanism we should use to call callback-methods in the main thread
     * @param signaturePattern signature string of the type(s) of ZG program you want to get information about.
     *                         May be wildcarded if you want to get information about multiple types of program.
     *                         (e.g. passing "*" will get you info about all kinds of ZG programs on the LAN)
     * @param optAdditionalCriteria optional reference to a QueryFilter that describes any additional criteria regarding
     *                          what sorts of server, in particular, you are interested in.  Servers whose
     *                          reply-Messages don't match this QueryFilter's criteria will not be included
     *                          in the discovered-servers-set.  May be NULL if you just want to receive everything.
     * @note be sure to call Start() to start the discovery-thread running!
     */
   SystemDiscoveryClient(ICallbackMechanism * mechanism, const String & signaturePattern, const ConstQueryFilterRef & optAdditionalCriteria = ConstQueryFilterRef());

   /** Destructor */
   ~SystemDiscoveryClient();

   /** Starts the discovery thread going.
     * @param pingIntervalMicroseconds how many microseconds should elapse between successive pings.
     *                                 Defaults to 500 milliseconds' worth.
     * @returns B_NO_ERROR on success, or an error code if setup failed.
     * @note if called while the thread is already running, the thread will be stopped and then restarted.
     */
   status_t Start(uint64 pingIntervalMicroseconds = MillisToMicros(500));

   /** Stops the discovery thread, if it is currently running. */
   void Stop();

   /** Returns our ping-interval, as was specified in the most recent call to Start().
     * If called while this module isn't running, returns 0.
     */
   uint64 GetPingIntervalMicroseconds() const {return _pingInterval;}

   /** Returns true if this module is currently active/pinging, or false if it is not. */
   bool IsActive() const {return (_pingInterval > 0);}

protected:
   virtual void DispatchCallbacks(uint32 eventTypeBits);

private:
   friend class IDiscoveryNotificationTarget;
   friend class DiscoveryImplementation;

   void RegisterTarget(IDiscoveryNotificationTarget * newTarget);
   void UnregisterTarget(IDiscoveryNotificationTarget * target);
   void MainThreadNotifyAllOfSleepChange(bool isAboutToSleep);

   uint64 _pingInterval;  // non-zero only after a call to Start()
   Hashtable<IDiscoveryNotificationTarget *, bool> _targets;  // value == is-new-target
   Hashtable<String, MessageRef> _knownInfos;

   DiscoveryImplementation * _imp;
   ConstQueryFilterRef _queryFilter;
};
DECLARE_REFTYPES(SystemDiscoveryClient);

};  // end namespace zg

#endif

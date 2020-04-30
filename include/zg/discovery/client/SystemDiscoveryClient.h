/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef SystemDiscoveryClient_h
#define SystemDiscoveryClient_h

#include "message/Message.h"
#include "regex/QueryFilter.h"
#include "zg/callback/ICallbackSubscriber.h"

namespace zg {

class SystemDiscoveryClient;
class DiscoveryImplementation;

/** Interface for an object that wants to be notified about the ZG systems' availability/un-availability */
class IDiscoveryNotificationTarget
{
public:
   /** Constructor
     * param discoveryClient pointer to the SystemDiscoveryClient to register with, or NULL if you want to call SetDiscoveryClient() explicitely later.
     */
   IDiscoveryNotificationTarget(SystemDiscoveryClient * discoveryClient) : _discoveryClient(NULL) {SetDiscoveryClient(discoveryClient);}

   /** Destructor.  Calls SetDiscoveryClient(NULL) to auto-unregister this object from the SystemDiscoveryClient if necessary. */
   virtual ~IDiscoveryNotificationTarget() {SetDiscoveryClient(NULL);}

   /** Unregisters this object from any SystemDiscoveryClient it's currently registered to 
     * (if necessary) and then registers it with (discoveryClient).
     * @param discoveryClient pointer to the SystemDiscoveryClient to register with, or NULL to unregister only.
     * @note if this IDiscoveryNotificationTarget is already registered with (discoveryClient),
     *       then no action is taken.
     */
   void SetDiscoveryClient(SystemDiscoveryClient * discoveryClient);

   /** Returns a pointer to the SystemDiscoveryClient we're currently registered with, or NULL
     * if we aren't currently registered with any SystemDiscoveryClient.
     */
   SystemDiscoveryClient * GetDiscoveryClient() const {return _discoveryClient;}

   /** Called when updated system-information has become available regarding a particular ZG system.
     * @param systemName the name of the ZG system that this update is describing.
     * @param optSystemInfo If non-NULL, this contains information about the ZG system.
     *                      If NULL, then this update means that the specified ZG system has gone away.
     */
   virtual void DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo) = 0;

   /** Called just before this computer goes into sleep mode.  Default implementation is a no-op. */
   virtual void ComputerIsAboutToSleep() {/* empty */}

   /** Called just after this computer comes out of sleep mode.  Default implementation is a no-op. */
   virtual void ComputerJustWokeUp() {/* empty */}

private:
   SystemDiscoveryClient * _discoveryClient;
};

/** This object is a front-end to the ZG system-discovery logic.
  * Instantiate one of these to get notifications about what ZG systems
  * are currently on-line.
  */
class SystemDiscoveryClient MUSCLE_FINAL_CLASS : public ICallbackSubscriber, public RefCountable
{
public:
   /** Constructor.
     * @param mechanism the CallbackMechanism we should use to call callback-methods in the main thread
     * @param optServerCriteria optional reference to a QueryFilter that describes the criteria regarding
     *                          what types of server in particular you are interested in.  Servers whose
     *                          reply-Messages don't match this QueryFilter's criteria will not be included
     *                          in the discovered-servers-set.  May be NULL if you just want to receive everything.
     * @note be sure to call Start() to start the discovery-thread running!
     */
   SystemDiscoveryClient(ICallbackMechanism * mechanism, const ConstQueryFilterRef & optServerCriteria);

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

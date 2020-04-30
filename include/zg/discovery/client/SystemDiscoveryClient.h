/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef DiscoveryModule_h
#define DiscoveryModule_h

#include "callback/ICallbackSubscriber.h"
#include "message/Message.h"

namespace zg {

class DiscoveryModule;
class DiscoveryImplementation;

/** Interface for an object that wants to be notified about the ZG systems' availability/un-availability */
class IDiscoveryNotificationTarget
{
public:
   /** Constructor
     * param discoveryModule pointer to the DiscoveryModule to register with, or NULL if you want to call SetDiscoveryModule() explicitely later.
     */
   IDiscoveryNotificationTarget(DiscoveryModule * discoveryModule) : _discoveryModule(NULL) {SetDiscoveryModule(discoveryModule);}

   /** Destructor.  Calls SetDiscoveryModule(NULL) to auto-unregister this object from the DiscoveryModule if necessary. */
   virtual ~IDiscoveryNotificationTarget() {SetDiscoveryModule(NULL);}

   /** Unregisters this object from any DiscoveryModule it's currently registered to 
     * (if necessary) and then registers it with (discoveryModule).
     * @param discoveryModule pointer to the module to register with, or NULL to unregister only.
     * @note if this IDiscoveryNotificationTarget is already registered with (discoveryModule),
     *       then no action is taken.
     */
   void SetDiscoveryModule(DiscoveryModule * discoveryModule);

   /** Returns a pointer to the DiscoveryModule we're currently registered with, or NULL
     * if we aren't currently registered with any DiscoveryModule.
     */
   DiscoveryModule * GetDiscoveryModule() const {return _discoveryModule;}

   /** Called when updated system-information has become available regarding a particular LCS  system.
     * @param systemName the name of the LCS  system that this update is describing.
     * @param optSystemInfo If non-NULL, this contains information about the LCS  system.
     *                      If NULL, then this update means that the specified LCS  system has gone away.
     */
   virtual void DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo) = 0;

   /** Called just before this computer goes into sleep mode.  Default implementation is a no-op. */
   virtual void ComputerIsAboutToSleep() {/* empty */}

   /** Called just after this computer comes out of sleep mode.  Default implementation is a no-op. */
   virtual void ComputerJustWokeUp() {/* empty */}

private:
   DiscoveryModule * _discoveryModule;
};

/** This module is a front-end to the LCS  system-discovery logic.
  * Instantiate one of these to get notifications about what LCS  systems
  * are currently on-line.
  */
class DiscoveryModule MUSCLE_FINAL_CLASS : public ICallbackSubscriber, public RefCountable
{
public:
   /** Constructor.
     * @param provider the CallbackProvider we should use to call callback-methods in the main thread
     * @note be sure to call Start() to start the discovery-thread running!
     */
   DiscoveryModule(ICallbackProvider * provider);

   /** Destructor */
   ~DiscoveryModule();

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
};
DECLARE_REFTYPES(DiscoveryModule);

};  // end namespace zg

#endif

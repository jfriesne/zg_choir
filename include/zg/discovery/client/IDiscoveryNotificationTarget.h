#ifndef IDiscoveryNotificationTarget_h
#define IDiscoveryNotificationTarget_h

#include "message/Message.h"
#include "zg/ZGNameSpace.h"

namespace zg {

class SystemDiscoveryClient;

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

};  // end namespace zg

#endif

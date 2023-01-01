#ifndef IDiscoveryServerSessionController_h
#define IDiscoveryServerSessionController_h

#include "zg/ZGNameSpace.h"
#include "message/Message.h"
#include "util/IPAddress.h"

namespace zg {

/** Interface to an object that wants to receive callbacks from a DiscoveryServerSession object */
class IDiscoveryServerSessionController
{
public:
   /** Constructor */
   IDiscoveryServerSessionController() {/* empty */}

   /** Destructor */
   virtual ~IDiscoveryServerSessionController() {/* empty */}

   /** Called whenever a discovery "ping" Message is received via multicast.
     * Should be implemented to update (pingMsg) to point to the corresponding "pong" Message
     * to send back via unicast.
     * @param pingMsg the received "ping" Message, and (on successful return) the "pong" Message to send back.
     * @param pingSource the IP address and port that this "ping" Message was sent from (and where
     *                   our "pong" reply will be sent back to)
     * @returns How many microseconds from now the "pong" Message should be sent back.
     *          eg return 0 to have the "pong" Message sent back ASAP, or MillisToMicros(100) to have
     *          it sent back after a 100mS delay, and so on.  Return MUSCLE_TIME_NEVER if you don't
     *          want the "pong" Message to ever be sent back.
     */
   virtual uint64 HandleDiscoveryPing(MessageRef & pingMsg, const IPAddressAndPort & pingSource) = 0;
};

};  // end namespace zg

#endif

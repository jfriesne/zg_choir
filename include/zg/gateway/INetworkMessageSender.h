#ifndef INetworkMessageSender_h
#define INetworkMessageSender_h

#include "message/Message.h"
#include "zg/ZGNameSpace.h"

namespace zg {

/** Interface class that can be inherited by any object that knows how to send a MessageRef out to the network */
class INetworkMessageSender
{
public:
   INetworkMessageSender() {/* empty */}
   virtual ~INetworkMessageSender() {/* empty */}

   /** Should be implemented to enqueue the specified MessageRef for sending to the network ASAP.
     * @param msg The Message to send.  This Message shouldn't be modified after this call returns.
     * @returns B_NO_ERROR if the Message was successfully enqueued for sending, or some other value if an error occurred.
     */
   virtual status_t SendOutgoingMessageToNetwork(const ConstMessageRef & msg) = 0;
};

};  // end namespace zg

#endif

/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef ClientConnector_h
#define ClientConnector_h

#include "message/Message.h"
#include "regex/QueryFilter.h"
#include "zg/callback/ICallbackSubscriber.h"
#include "zg/gateway/INetworkMessageSender.h"

namespace zg {

class ClientConnectorImplementation;

/** Abstract front-end to ZG's TCP-client-connector logic.
  * It handles setting up, maintaining, and (if necessary) reconnecting a TCP connection to one of the ZG system's servers. 
  * Any required functionality while the TCP connection is active is delegated to a concrete subclass.
  */
class ClientConnector : public ICallbackSubscriber, public INetworkMessageSender, public RefCountable
{
public:
   /** Constructor.
     * @param mechanism the CallbackMechanism we should use to call callback-methods in the main thread
     * @param signaturePattern signature string of the type(s) of ZG program you want to connect to.
     *                         May be wildcarded if you aren't particular about the type of server you want to connect to.
     *                         (e.g. passing "*" will allow you to connect to any kind of ZG server)
     * @param systemNamePattern Name of the ZG system we want to connect to.  May be wildcarded if you aren't particular about
     *                          the name of the system you connect to.  (e.g. passing "*" will get you connected to a system
     *                          regardless of what its system-name is)
     * @param optAdditionalCriteria optional reference to a QueryFilter that describes any additional criteria regarding
     *                          what sorts of server, in particular, you are interested in.  Servers whose
     *                          reply-Messages don't match this QueryFilter's criteria will not be connected to.
     *                          May be NULL if you only care about the server's signature and system name.
     * @note be sure to call Start() to start the network I/O thread running!
     */
   ClientConnector(ICallbackMechanism * mechanism, const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalCriteria = ConstQueryFilterRef());

   /** Destructor */
   ~ClientConnector();

   /** Starts the network I/O thread.
     * @param autoReconnectTimeMicroseconds how long to wait before trying to auto-reconnect, when our TCP connection gets severed.  Defaults to 250 milliseconds.
     * @returns B_NO_ERROR on success, or an error code if setup failed.
     * @note if called while the thread is already running, the thread will be stopped and then restarted.
     */
   status_t Start(uint64 autoReconnectTimeMicroseconds = MillisToMicros(250));

   /** Stops the network I/O thread, if it is currently running. */
   void Stop();

   /** Returns true if this connector's I/O thread is currently started, or false if it is not. */
   bool IsActive() const;

   /** Returns true our TCP connection to the server is currently connected */
   bool IsConnected() const;

protected:
   virtual void DispatchCallbacks(uint32 eventTypeBits);

   /** May be overridden with a custom method for extracting the TCP port-number to connect to from the given discovery-results Message.
     * @param msg The Message to parse
     * @param retPort on success, the TCP port number should be written here.
     * @returns B_NO_ERROR on success, or some other error code on failure.
     * @note the default implementation just calls msg.FindInt16("port", retPort)
     * @note this method will be called from the network I/O thread, so be careful about accessing any data other than the passed-in arguments.
     */
   virtual status_t ParseTCPPortFromMessage(const Message & msg, uint16 & retPort) const;

   /** Called when our connection-status changes.
     * @param optServerInfo if non-NULL, information about the ZG server we have connected to.
     *                      If we have just disconnected, then this will be a NULL MessageRef().
     */
   virtual void ConnectionStatusUpdated(const MessageRef & optServerInfo) = 0;

   /** Called when a Message has been received from the TCP connection
     * @param msg Reference to the received Message
     */
   virtual void MessageReceivedFromNetwork(const MessageRef & msg) = 0;

   // INetworkMessageSender API -- sends (msg) out over our TCP connection
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg);

private:
   friend class ClientConnectorImplementation;

   ClientConnectorImplementation * _imp;
};
DECLARE_REFTYPES(ClientConnector);

};  // end namespace zg

#endif

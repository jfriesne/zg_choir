#ifndef MessageTreeClientConnector_h
#define MessageTreeClientConnector_h

#include "zg/connector/ClientConnector.h"
#include "zg/messagetree/client/ClientSideNetworkTreeGateway.h"
#include "zg/messagetree/gateway/MuxTreeGateway.h"

namespace zg {

/** This class handles the TCP connection to a ZG server that uses the MessageTree protocol. */
class MessageTreeClientConnector : public ClientConnector, public MuxTreeGateway
{
public:
   /** Constructor.
     * @param mechanism the CallbackMechanism we should use to call callback-methods in the main thread
     * @note be sure to call Start() to start the network I/O thread running, otherwise this object won't do anything useful.
     */
   MessageTreeClientConnector(ICallbackMechanism * mechanism);

   /** Destructor */
   virtual ~MessageTreeClientConnector();

   /** Returns the undo-key string that this connector will use to tag its transactions on the server.
     * This key is arbitrary, but typically it should be unique to a particular user, so that that user
     * can undo his own transactions but nobody else's.
     * If you don't call SetUndoKey(), a randomly generated string will be used (and this method will return that string).
     */
   const String & GetUndoKey() const {return _undoKey;}

   /** Call this if you want to specify the undo-key to use.  You might want to call this e.g. if
     * you are making multiple TCP connections to the server and you want them all to share the
     * same undo-stack.  Note that you should call this method before calling Start(), since the
     * undo-key will be sent to the server when the TCP connection is made.
     * @param undoKey the new undo-key string to use.
     */
   void SetUndoKey(const String & undoKey) {_undoKey = undoKey;}

protected:
   virtual void ConnectionStatusUpdated(const MessageRef & optServerInfo);
   virtual void MessageReceivedFromNetwork(const MessageRef & msg);

private:
   ClientSideNetworkTreeGateway _networkGateway;

   String _undoKey;
   bool _expectingParameters;
};
DECLARE_REFTYPES(MessageTreeClientConnector);

};  // end namespace zg

#endif

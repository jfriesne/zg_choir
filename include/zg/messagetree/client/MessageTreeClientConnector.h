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
   MessageTreeClientConnector(ICallbackMechanism * mechanism, const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalCriteria = ConstQueryFilterRef());

   /** Destructor */
   virtual ~MessageTreeClientConnector();

protected:
   virtual void ConnectionStatusUpdated(const MessageRef & optServerInfo);
   virtual void MessageReceivedFromNetwork(const MessageRef & msg);

private:
   ClientSideNetworkTreeGateway _networkGateway;
};
DECLARE_REFTYPES(MessageTreeClientConnector);

};  // end namespace zg

#endif

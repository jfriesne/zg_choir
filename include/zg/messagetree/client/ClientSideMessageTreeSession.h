#ifndef ClientSideMessageTreeSession_h
#define ClientSideMessageTreeSession_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "zg/messagetree/client/ClientSideNetworkTreeGateway.h"
#include "reflector/AbstractReflectSession.h"

namespace zg {

/** This class is a StorageReflectSession that functions as a connected client's interface to a
  * server that is implementing a database.
  */
class ClientSideMessageTreeSession : public AbstractReflectSession, public MuxTreeGateway, private INetworkMessageSender
{
public:
   ClientSideMessageTreeSession();
   virtual ~ClientSideMessageTreeSession();

   virtual void AsyncConnectCompleted();
   virtual bool ClientConnectionClosed();
   virtual void AboutToDetachFromServer();

private:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return AddOutgoingMessage(msg);}

   ClientSideNetworkTreeGateway _networkGateway;
};
DECLARE_REFTYPES(ClientSideMessageTreeSession);

};  // end namespace zg

#endif

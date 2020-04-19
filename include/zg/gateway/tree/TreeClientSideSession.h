#ifndef TreeClientSideSession_h
#define TreeClientSideSession_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/gateway/tree/MuxTreeGateway.h"
#include "zg/gateway/tree/NetworkTreeGateway.h"
#include "reflector/AbstractReflectSession.h"

namespace zg {

/** This class is a StorageReflectSession that functions as a connected client's interface to a
  * server that is implementing a database.
  */
class TreeClientSideSession : public AbstractReflectSession, public MuxTreeGateway, private INetworkMessageSender
{
public:
   TreeClientSideSession();
   virtual ~TreeClientSideSession();

   virtual void AsyncConnectCompleted();
   virtual bool ClientConnectionClosed();
   virtual void AboutToDetachFromServer();

private:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return AddOutgoingMessage(msg);}

   ClientSideNetworkTreeGateway _networkGateway;
};
DECLARE_REFTYPES(TreeClientSideSession);

};  // end namespace zg

#endif

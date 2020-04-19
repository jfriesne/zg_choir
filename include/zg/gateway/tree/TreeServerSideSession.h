#ifndef TreeServerSideSession_h
#define TreeServerSideSession_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/gateway/tree/NetworkTreeGateway.h"
#include "reflector/StorageReflectSession.h"

namespace zg {

/** This class is a StorageReflectSession that functions as a connected client's interface to a
  * server that is implementing a database.
  */
class TreeServerSideSession : public StorageReflectSession, public ServerSideNetworkTreeGatewaySubscriber, private INetworkMessageSender
{
public:
   TreeServerSideSession(ITreeGateway * upstreamGateway);
   virtual ~TreeServerSideSession();

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

protected:
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return AddOutgoingMessage(msg);}
};
DECLARE_REFTYPES(TreeServerSideSession);

/** This class is a factory that creates a new TreeServerSideSession object when an incoming TCP connection is received */
class TreeServerSideSessionFactory : public ReflectSessionFactory, public ITreeGatewaySubscriber
{
public:
   TreeServerSideSessionFactory(ITreeGateway * upstreamGateway);

   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);

   virtual bool IsReadyToAcceptSessions() const {return IsTreeGatewayConnected();}
};
DECLARE_REFTYPES(TreeServerSideSessionFactory);

};  // end namespace zg

#endif

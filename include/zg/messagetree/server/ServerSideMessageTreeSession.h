#ifndef ServerSideMessageTreeSession_h
#define ServerSideMessageTreeSession_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/messagetree/server/ServerSideNetworkTreeGatewaySubscriber.h"
#include "reflector/StorageReflectSession.h"

namespace zg {

/** This class is a StorageReflectSession that functions as a connected client's interface to a
  * server that is implementing a database.
  */
class ServerSideMessageTreeSession : public StorageReflectSession, public ServerSideNetworkTreeGatewaySubscriber, private INetworkMessageSender
{
public:
   ServerSideMessageTreeSession(ITreeGateway * upstreamGateway);
   virtual ~ServerSideMessageTreeSession();

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

protected:
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return AddOutgoingMessage(msg);}
};
DECLARE_REFTYPES(ServerSideMessageTreeSession);

/** This class is a factory that creates a new ServerSideMessageTreeSession object when an incoming TCP connection is received */
class ServerSideMessageTreeSessionFactory : public ReflectSessionFactory, public ITreeGatewaySubscriber
{
public:
   ServerSideMessageTreeSessionFactory(ITreeGateway * upstreamGateway);

   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);

   virtual bool IsReadyToAcceptSessions() const {return IsTreeGatewayConnected();}
};
DECLARE_REFTYPES(ServerSideMessageTreeSessionFactory);

};  // end namespace zg

#endif

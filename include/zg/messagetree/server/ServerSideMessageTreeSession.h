#ifndef ServerSideMessageTreeSession_h
#define ServerSideMessageTreeSession_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/messagetree/server/ServerSideNetworkTreeGatewaySubscriber.h"
#include "reflector/StorageReflectSession.h"
#include "util/NestCount.h"

namespace zg {

/** This class is a StorageReflectSession that functions as one connected client's interface
  *  to a server that is implementing a database.  It runs inside a ReflectServer inside a server process.
  */
class ServerSideMessageTreeSession : public StorageReflectSession, public ServerSideNetworkTreeGatewaySubscriber, private INetworkMessageSender
{
public:
   /** Constructor
     * @param upstreamGateway the ITreeGateway we will access to request database-update operations 
     */
   ServerSideMessageTreeSession(ITreeGateway * upstreamGateway);

   /** Destructor */
   virtual ~ServerSideMessageTreeSession();

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

   /** Returns true iff we are currently executing inside our MessageReceivedFromGateway callback */
   bool IsInMessageReceivedFromGateway() const {return _isInMessageReceivedFromGateway.IsInBatch();}

protected:
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return AddOutgoingMessage(msg);}

   // These are overridden here so that we can tap directly into the MUSCLE-level subscription mechanisms
   // rather than relying on our upstream ITreeGateway to handle subscription.  That way we can easily support
   // node-paths that span across multiple ZG databases.
   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveAllTreeSubscriptions(TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags = TreeGatewayFlags());

private:
   NestCount _isInMessageReceivedFromGateway;
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

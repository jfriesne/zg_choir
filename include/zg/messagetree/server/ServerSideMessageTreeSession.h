#ifndef ServerSideMessageTreeSession_h
#define ServerSideMessageTreeSession_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/messagetree/server/ServerSideNetworkTreeGatewaySubscriber.h"
#include "reflector/StorageReflectSession.h"
#include "util/NestCount.h"

namespace zg {

class ClientDataMessageTreeDatabaseObject;

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

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

   /** Returns true iff we are currently executing inside our MessageReceivedFromGateway callback */
   bool IsInMessageReceivedFromGateway() const {return _isInMessageReceivedFromGateway.IsInBatch();}

   /** If set true, then we will write an informative message to the log from within our AttachedToServer() and AboutToDetachFromServer() methods
     * @param doLogging true iff logging is desired.
     * Default state is false.
     */
   void SetLogOnAttachAndDetach(bool doLogging) {_logOnAttachAndDetach = doLogging;}

   /** Returns the key-string for the undo-stack this session's commands should be applied to (when using an UndoStackMessageTreeDatabaseObject) */
   const String & GetUndoKey() const {return _undoKey;}

protected:
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return AddOutgoingMessage(msg);}

   // These are overridden here so that we can tap directly into the MUSCLE-level subscription mechanisms
   // rather than relying on our upstream ITreeGateway to handle subscription.  That way we can easily support
   // node-paths that span across multiple ZG databases.
   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveAllTreeSubscriptions(TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth = MUSCLE_NO_LIMIT, TreeGatewayFlags flags = TreeGatewayFlags());

private:
   friend class ClientDataMessageTreeDatabaseObject;

   NestCount _isInMessageReceivedFromGateway;
   bool _logOnAttachAndDetach;

   String _undoKey;
};
DECLARE_REFTYPES(ServerSideMessageTreeSession);

/** This class is a factory that creates a new ServerSideMessageTreeSession object when an incoming TCP connection is received */
class ServerSideMessageTreeSessionFactory : public ReflectSessionFactory, public ITreeGatewaySubscriber
{
public:
   /** Constructor
     * @param upstreamGateway the ITreeGateway that client connections should use to access the ZG database
     * @param announceClientConnectsAndDisconnects if true, we'll write to the log whenever a client connects or disconnects.  Defaults to false.
     */
   ServerSideMessageTreeSessionFactory(ITreeGateway * upstreamGateway, bool announceClientConnectsAndDisconnects = false);

   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);

   virtual bool IsReadyToAcceptSessions() const {return IsTreeGatewayConnected();}

private:
   bool _announceClientConnectsAndDisconnects;
};
DECLARE_REFTYPES(ServerSideMessageTreeSessionFactory);

};  // end namespace zg

#endif

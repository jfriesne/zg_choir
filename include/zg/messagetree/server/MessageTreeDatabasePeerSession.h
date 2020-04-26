#ifndef MessageTreeDatabasePeerSession_h
#define MessageTreeDatabasePeerSession_h

#include "zg/ZGDatabasePeerSession.h"
#include "zg/IDatabaseObject.h"
#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "zg/messagetree/gateway/ProxyTreeGateway.h"

namespace zg
{

class MessageTreeDatabaseObject;

/** This is a ZGDatabasePeerSession that has been further specialized to be able to
  * support MUSCLE node-tree database semantics in particular.
  */
class MessageTreeDatabasePeerSession : public ZGDatabasePeerSession, private ProxyTreeGateway
{
public:
   /** Constructor
     * @param peerSettings the ZGPeerSettings that this system is to use.
     */
   MessageTreeDatabasePeerSession(const ZGPeerSettings & peerSettings);

   /** Returns a pointer to the ITreeGateway object that our clients should use to access the ZG-synchronized database data */
   ITreeGateway * GetClientTreeGateway() {return &_muxGateway;}

protected:
   /** This will be called as part of the startup sequence.  It should create
     * a new IDatabaseObject that will represent the specified database and return
     * a reference to it, for the MessageTreeDatabasePeerSession to manage.
     * @param whichDatabase The index of the database that we need an object to represent.
     */
   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase) = 0;

   // Overridden to call PushSubscriptionMessages()
   virtual void CommandBatchEnds();

   /** Given a nodePath, returns the associated MessageTreeDatabaseObject, or NULL if none matches.
     * @param nodePath a node-path to check (either absolute or session-relative)
     * @param optRetRelativePath if non-NULL, then on success, a database-object-relative sub-path will be written here.
     * @returns a pointer to the appropriate MessageTreeDatabaseObject on success, or NULL on failure (no matching DB found)
     */
   MessageTreeDatabaseObject * GetDatabaseForNodePath(const String & nodePath, String * optRetRelativePath);

   // ZGPeerSession API implementation
   virtual void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo);

   // ITreeGateway API implementation
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore);
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, uint32 whichDB, const String & tag, TreeGatewayFlags flags);
   virtual bool TreeGateway_IsGatewayConnected() const {return IAmFullyAttached();}

   // StorageReflectSession API implementation
   virtual String GenerateHostName(const IPAddress &, const String &) const {return "zg";}
   virtual void NotifySubscribersThatNodeChanged(DataNode & node, const MessageRef & oldData, bool isBeingRemoved);
   virtual void NotifySubscribersThatNodeIndexChanged(DataNode & node, char op, uint32 index, const String & key);

   // ZGPeerSession API implementation
   virtual ConstMessageRef SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg);

private:
   friend class MessageTreeDatabaseObject;

   void HandleSeniorPeerPingMessage(uint32 whichDatabase, const ConstMessageRef & msg);

   Queue<IDatabaseObjectRef> _databaseObjects;

   MuxTreeGateway _muxGateway;
};
DECLARE_REFTYPES(MessageTreeDatabasePeerSession);

};  // end namespace zg

#endif

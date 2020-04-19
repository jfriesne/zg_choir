#ifndef ZGMessageTreeDatabasePeerSession_h
#define ZGMessageTreeDatabasePeerSession_h

#include "zg/ZGDatabasePeerSession.h"
#include "zg/IDatabaseObject.h"
#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "zg/messagetree/gateway/ProxyTreeGateway.h"

namespace zg
{

class ZGMessageTreeDatabaseObject;

/** This is a ZGDatabasePeerSession that has been further specialized to be able to
  * support MUSCLE node-tree database semantics in particular.
  */
class ZGMessageTreeDatabasePeerSession : public ZGDatabasePeerSession, private ProxyTreeGateway
{
public:
   /** Constructor
     * @param peerSettings the ZGPeerSettings that this system is to use.
     */
   ZGMessageTreeDatabasePeerSession(const ZGPeerSettings & peerSettings);

   /** Returns a pointer to the ITreeGateway object that our clients should use to access the ZG-synchronized database data */
   ITreeGateway * GetClientTreeGateway() {return &_muxGateway;}

protected:
   /** This will be called as part of the startup sequence.  It should create
     * a new IDatabaseObject that will represent the specified database and return
     * a reference to it, for the ZGMessageTreeDatabasePeerSession to manage.
     * @param whichDatabase The index of the database that we need an object to represent.
     */
   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase) = 0;

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
   virtual bool TreeGateway_IsGatewayConnected() const {return IAmFullyAttached();}

private:
   friend class ZGMessageTreeDatabaseObject;

   Queue<IDatabaseObjectRef> _databaseObjects;

   MuxTreeGateway _muxGateway;
};
DECLARE_REFTYPES(ZGMessageTreeDatabasePeerSession);

};  // end namespace zg

#endif

#ifndef ProxyTreeGateway_h
#define ProxyTreeGateway_h

#include "zg/messagetree/gateway/ITreeGateway.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

namespace zg {

/** This class just forwards on all requests from its downstream ITreeGatewaySubscribers to its upstream ITreeGateway,
  * and all replies from its upstream ITreeGateway back to its downstream ITreeGatewaySubscribers.
  * It's not that useful on its own, but rather is generally used as a starting point to subclass from.
  */
class ProxyTreeGateway : public ITreeGateway, public ITreeGatewaySubscriber
{
public:
   ProxyTreeGateway(ITreeGateway * optUpstreamGateway);

   virtual ~ProxyTreeGateway();

   virtual void ShutdownGateway();

public:
   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeServerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeGatewayShuttingDown();

protected:
   // IGateway function-call API
   virtual void CommandBatchBegins();
   virtual void CommandBatchEnds();

   // IGatewaySubscriber callback API
   virtual void CallbackBatchBegins();
   virtual void CallbackBatchEnds();

   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore);
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags);
   virtual bool TreeGateway_IsGatewayConnected() const;
};

};  // end namespace zg

#endif

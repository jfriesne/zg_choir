#ifndef MuxTreeGateway_h
#define MuxTreeGateway_h

#include "zg/gateway/tree/ProxyTreeGateway.h"

namespace zg {

class ITreeGatewaySubscriber;

/** This class contains the logic to multiplex the requests of multiple ITreeGatewaySubscriber objects
  * into a single request-stream for an upstream ITreeGateway to handle, and to demultiplex the results
  * coming back from the upstream ITreeGateway for our own ITreeGatewaySubscriber objects.
  */
class MuxTreeGateway : public ProxyTreeGateway
{
public:
   MuxTreeGateway(ITreeGateway * optUpstreamGateway) : ProxyTreeGateway(upstreamGateway) {/* empty */}
   virtual ~MuxTreeGateway() {/* empty */}

   virtual void ShutdownGateway();

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy); 
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore);
   virtual status_t TreeGateway_UploadNodeValues(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual bool TreeGateway_IsGatewayConnected() const;
   
   // ITreeGatewaySubscriber callback API
   virtual void TreeCallbackBatchBeginning();
   virtual void TreeCallbackBatchEnding();
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeServerPonged(const String & tag);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeGatewayShuttingDown();
};

};  // end namespace zg

#endif

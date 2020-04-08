#ifndef ITreeGatewaySubscriber_h
#define ITreeGatewaySubscriber_h

#include "zg/gateway/tree/DummyTreeGateway.h"

namespace zg {

/** Abstract base class for objects that want to connect to an ITreeGateway as downstream clients */
class ITreeGatewaySubscriber : public IGatewaySubscriber
{
public:
   ITreeGatewaySubscriber(ITreeGateway * gateway) : IGatewaySubscriber(gateway) {/* empty */}
   virtual ~ITreeGatewaySubscriber() {/* empty */}

   void SetTreeGateway(ITreeGateway * gateway) {IGatewaySubscriber::SetGateway(gateway);}
   ITreeGateway * GetTreeGateway() const {ITreeGateway * gw = static_cast<ITreeGateway *>(GetGateway()); return gw ? gw : GetDummyTreeGateway();}

public:
   // ITreeGatewaySubscriber callback API

   virtual void TreeCallbackBatchBeginning() {/* empty */}
   virtual void TreeCallbackBatchEnding() {/* empty */}

   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg) {(void) nodePath; (void) payloadMsg;}
   virtual void TreeNodeIndexCleared(const String & path) {(void) path;}
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName) {(void) path; (void) insertedAtIndex; (void) nodeName;}
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName) {(void) path; (void) removedAtIndex; (void) nodeName;}
   virtual void TreeServerPonged(const String & tag) {(void) tag;}

   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData) {(void) tag; (void) subtreeData;}

   virtual void TreeGatewayConnectionStateChanged() {/* empty */}
   virtual void TreeGatewayShuttingDown() {/* empty */}

protected:
   // Function-call API -- passed through to our ITreeGateway's function-call API
   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_AddSubscription(this, subscriptionPath, optFilterRef, flags);}
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef) {return GetTreeGateway()->TreeGateway_RemoveSubscription(this, subscriptionPath, optFilterRef);}
   virtual status_t RemoveAllTreeSubscriptions() {return GetTreeGateway()->TreeGateway_RemoveAllSubscriptions(this);}

   virtual status_t RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_RequestNodeValues(this, queryString, optFilterRef, flags);}
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_RequestNodeSubtrees(this, queryStrings, queryFilters, tag, maxDepth, flags);}

   virtual status_t UploadTreeNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags = TreeGatewayFlags(), const char * optBefore = NULL) {return GetTreeGateway()->TreeGateway_UploadNodeValue(this, path, optPayload, flags, optBefore);}
   virtual status_t UploadTreeNodeValues(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_UploadNodeValues(this, basePath, valuesMsg, flags);}

   virtual status_t RequestDeleteTreeNodes(const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_RequestDeleteNodes(this, path, optFilterRef, flags);}
   virtual status_t RequestMoveTreeIndexEntry(const String & path, const char * optBefore, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_RequestMoveIndexEntry(this, path, optBefore, flags);}

   virtual status_t PingTreeServer(const String & tag, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->TreeGateway_PingServer(this, tag, flags);}
   virtual bool IsTreeGatewayConnected() const {return GetTreeGateway()->TreeGateway_IsGatewayConnected();}

private:
   friend class ITreeGateway;
};

};  // end namespace zg

#endif

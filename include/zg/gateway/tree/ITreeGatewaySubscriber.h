#ifndef ITreeGatewaySubscriber_h
#define ITreeGatewaySubscriber_h

#include "support/BitChord.h"
#include "message/Message.h"
#include "regex/QueryFilter.h"
#include "zg/gateway/IGatewaySubscriber.h"

namespace zg {

class ITreeGateway;

/** Flags used to optionally modify the behavior of ITreeGateway commands */
enum {
   TREE_GATEWAY_FLAG_INDEXED = 0,  /**< If set, the uploaded node should be added to it's parent's ordered-nodes index. */
   TREE_GATEWAY_FLAG_NOREPLY,      /**< If set, no initial reply is desired */
   TREE_GATEWAY_FLAG_TOSENIOR,     /**< If set in a call to PingTreeServer(), the ping will travel to the senior peer before coming back */
   NUM_TREE_GATEWAY_FLAGS          /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(TreeGatewayFlags, NUM_TREE_GATEWAY_FLAGS);

class ITreeGateway;

/** Abstract base class for objects that want to connect to an ITreeGateway as downstream clients */
class ITreeGatewaySubscriber : public IGatewaySubscriber<ITreeGateway>
{
public:
   ITreeGatewaySubscriber(ITreeGateway * gateway) : IGatewaySubscriber<ITreeGateway>(gateway) {/* empty */}
   virtual ~ITreeGatewaySubscriber() {/* empty */}

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
   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveAllTreeSubscriptions(TreeGatewayFlags flags = TreeGatewayFlags());

   virtual status_t RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags = TreeGatewayFlags());

   virtual status_t UploadTreeNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags = TreeGatewayFlags(), const char * optBefore = NULL);
   virtual status_t UploadTreeNodeSubtree(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags = TreeGatewayFlags());

   virtual status_t RequestDeleteTreeNodes(const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestMoveTreeIndexEntry(const String & path, const char * optBefore, TreeGatewayFlags flags = TreeGatewayFlags());

   virtual status_t PingTreeServer(const String & tag, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual bool IsTreeGatewayConnected() const;

private:
   friend class ITreeGateway;
};

};  // end namespace zg

#endif

#ifndef ITreeGateway_h
#define ITreeGateway_h

#include "zg/gateway/IGateway.h"
#include "message/Message.h"
#include "support/BitChord.h"
#include "regex/QueryFilter.h"
#include "util/String.h"

namespace zg {

class ITreeGatewaySubscriber;

/** Flags used to optionally modify the behavior of ITreeGateway commands */
enum {
   TREE_GATEWAY_FLAG_INDEXED = 0,  /**< If set, the uploaded node should be added to it's parent's ordered-nodes index. */
   TREE_GATEWAY_FLAG_NOREPLY,      /**< If set, no initial reply is desired */
   TREE_GATEWAY_FLAG_TOSENIOR,     /**< If set in a call to PingTreeServer(), the ping will travel to the senior peer before coming back */
   NUM_TREE_GATEWAY_FLAGS          /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(TreeGatewayFlags, NUM_TREE_GATEWAY_FLAGS);

/** Abstract base class for objects that want to implement the ITreeGateway API */
class ITreeGateway : public IGateway
{
public:
   ITreeGateway() {/* empty */}
   virtual ~ITreeGateway() {/* empty */}

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef) = 0;
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy) = 0;
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags) = 0;
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore) = 0;
   virtual status_t TreeGateway_UploadNodeValues(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags) = 0;
   virtual bool TreeGateway_IsGatewayConnected() const = 0;

private:
   friend class ITreeGatewaySubscriber;
};

};  // end namespace zg

#endif

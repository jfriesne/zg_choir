#include "zg/messagetree/gateway/ITreeGateway.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

namespace zg {

const char * _treeGatewayFlagLabels[] = {
   "Indexed",
   "NoReply",
   "Interim",
   "Sync",
   "DontCreateNode",
   "DontOverwriteData",
   "EnableSupercede",
   "TraverseSymlink",
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_treeGatewayFlagLabels, NUM_TREE_GATEWAY_FLAGS);

status_t ITreeGatewaySubscriber :: AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return GetGatewayOrDummyGateway()->TreeGateway_AddSubscription(this, subscriptionPath, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RemoveSubscription(this, subscriptionPath, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RemoveAllTreeSubscriptions(TreeGatewayFlags flags)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RemoveAllSubscriptions(this, flags);
}

status_t ITreeGatewaySubscriber :: RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & tag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RequestNodeValues(this, queryString, optFilterRef, flags, tag);
}

status_t ITreeGatewaySubscriber :: RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RequestNodeSubtrees(this, queryStrings, queryFilters, tag, maxDepth, flags);
}

status_t ITreeGatewaySubscriber :: UploadTreeNodeValue(const String & path, const ConstMessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_UploadNodeValue(this, path, optPayload, flags, optBefore, optOpTag);
}

status_t ITreeGatewaySubscriber :: UploadTreeNodeSubtree(const String & basePath, const ConstMessageRef & valuesMsg, TreeGatewayFlags flags, const String & optOpTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_UploadNodeSubtree(this, basePath, valuesMsg, flags, optOpTag);
}

status_t ITreeGatewaySubscriber :: RequestDeleteTreeNodes(const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RequestDeleteNodes(this, path, optFilterRef, flags, optOpTag);
}

status_t ITreeGatewaySubscriber :: RequestMoveTreeIndexEntry(const String & path, const String & optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RequestMoveIndexEntry(this, path, optBefore, optFilterRef, flags, optOpTag);
}

status_t ITreeGatewaySubscriber :: PingTreeLocalPeer(const String & tag, TreeGatewayFlags flags)
{
   return GetGatewayOrDummyGateway()->TreeGateway_PingLocalPeer(this, tag, flags);
}

status_t ITreeGatewaySubscriber :: PingTreeSeniorPeer(const String & tag, uint32 whichDB, TreeGatewayFlags flags)
{
   return GetGatewayOrDummyGateway()->TreeGateway_PingSeniorPeer(this, tag, whichDB, flags);
}

status_t ITreeGatewaySubscriber :: SendMessageToTreeSeniorPeer(const ConstMessageRef & msg, uint32 whichDB, const String & optTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_SendMessageToSeniorPeer(this, msg, whichDB, optTag);
}

status_t ITreeGatewaySubscriber :: SendMessageToSubscriber(const String & subscriberPath, const ConstMessageRef & msg, const ConstQueryFilterRef & optFilterRef, const String & tag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_SendMessageToSubscriber(this, subscriberPath, msg, optFilterRef, tag);
}

status_t ITreeGatewaySubscriber :: BeginUndoSequence(const String & optSequenceLabel, uint32 whichDB)
{
   return GetGatewayOrDummyGateway()->TreeGateway_BeginUndoSequence(this, optSequenceLabel, whichDB);
}

status_t ITreeGatewaySubscriber :: EndUndoSequence(const String & optSequenceLabel, uint32 whichDB)
{
   return GetGatewayOrDummyGateway()->TreeGateway_EndUndoSequence(this, optSequenceLabel, whichDB);
}

status_t ITreeGatewaySubscriber :: RequestUndo(uint32 whichDB, const String & optOpTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RequestUndo(this, whichDB, optOpTag);
}

status_t ITreeGatewaySubscriber :: RequestRedo(uint32 whichDB, const String & optOpTag)
{
   return GetGatewayOrDummyGateway()->TreeGateway_RequestRedo(this, whichDB, optOpTag);
}

bool ITreeGatewaySubscriber :: IsTreeGatewayConnected() const
{
   return GetGatewayOrDummyGateway()->TreeGateway_IsGatewayConnected();
}

ConstMessageRef ITreeGatewaySubscriber :: GetGestaltMessage() const
{
   return GetGatewayOrDummyGateway()->TreeGateway_GetGestaltMessage();
}

}  // end namespace zg

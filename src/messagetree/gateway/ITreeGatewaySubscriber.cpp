#include "zg/messagetree/gateway/ITreeGateway.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

namespace zg {

status_t ITreeGatewaySubscriber :: AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_AddSubscription(this, subscriptionPath, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RemoveSubscription(this, subscriptionPath, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RemoveAllTreeSubscriptions(TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RemoveAllSubscriptions(this, flags);
}

status_t ITreeGatewaySubscriber :: RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestNodeValues(this, queryString, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestNodeSubtrees(this, queryStrings, queryFilters, tag, maxDepth, flags);
}

status_t ITreeGatewaySubscriber :: UploadTreeNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore) 
{
   return GetGateway()->TreeGateway_UploadNodeValue(this, path, optPayload, flags, optBefore);
}

status_t ITreeGatewaySubscriber :: UploadTreeNodeSubtree(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_UploadNodeSubtree(this, basePath, valuesMsg, flags);
}

status_t ITreeGatewaySubscriber :: RequestDeleteTreeNodes(const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestDeleteNodes(this, path, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RequestMoveTreeIndexEntry(const String & path, const String * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestMoveIndexEntry(this, path, optBefore, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: PingTreeLocalPeer(const String & tag, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_PingLocalPeer(this, tag, flags);
}

status_t ITreeGatewaySubscriber :: PingTreeSeniorPeer(const String & tag, uint32 whichDB, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_PingSeniorPeer(this, tag, whichDB, flags);
}

status_t ITreeGatewaySubscriber :: SendMessageToTreeSeniorPeer(const MessageRef & msg, uint32 whichDB, const String & optTag) 
{
   return GetGateway()->TreeGateway_SendMessageToSeniorPeer(this, msg, whichDB, optTag);
}

status_t ITreeGatewaySubscriber :: BeginUndoSequence(const String & optSequenceLabel, uint32 whichDB)
{
   return GetGateway()->TreeGateway_BeginUndoSequence(this, optSequenceLabel, whichDB);
}

status_t ITreeGatewaySubscriber :: EndUndoSequence(const String & optSequenceLabel, uint32 whichDB)
{
   return GetGateway()->TreeGateway_EndUndoSequence(this, optSequenceLabel, whichDB);
}

status_t ITreeGatewaySubscriber :: RequestUndo(uint32 whichDB)
{
   return GetGateway()->TreeGateway_RequestUndo(this, whichDB);
}

status_t ITreeGatewaySubscriber :: RequestRedo(uint32 whichDB)
{
   return GetGateway()->TreeGateway_RequestRedo(this, whichDB);
}

bool ITreeGatewaySubscriber :: IsTreeGatewayConnected() const 
{
   return GetGateway()->TreeGateway_IsGatewayConnected();
}

};

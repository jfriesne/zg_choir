#include "zg/gateway/tree/ITreeGateway.h"
#include "zg/gateway/tree/ITreeGatewaySubscriber.h"

namespace zg {

status_t ITreeGatewaySubscriber :: AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_AddSubscription(this, subscriptionPath, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef) 
{
   return GetGateway()->TreeGateway_RemoveSubscription(this, subscriptionPath, optFilterRef);
}

status_t ITreeGatewaySubscriber :: RemoveAllTreeSubscriptions() 
{
   return GetGateway()->TreeGateway_RemoveAllSubscriptions(this);
}

status_t ITreeGatewaySubscriber :: RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestNodeValues(this, queryString, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestNodeSubtrees(this, queryStrings, queryFilters, tag, maxDepth, flags);
}

status_t ITreeGatewaySubscriber :: UploadTreeNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore) 
{
   return GetGateway()->TreeGateway_UploadNodeValue(this, path, optPayload, flags, optBefore);
}

status_t ITreeGatewaySubscriber :: UploadTreeNodeValues(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_UploadNodeValues(this, basePath, valuesMsg, flags);
}

status_t ITreeGatewaySubscriber :: RequestDeleteTreeNodes(const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestDeleteNodes(this, path, optFilterRef, flags);
}

status_t ITreeGatewaySubscriber :: RequestMoveTreeIndexEntry(const String & path, const char * optBefore, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_RequestMoveIndexEntry(this, path, optBefore, flags);
}

status_t ITreeGatewaySubscriber :: PingTreeServer(const String & tag, TreeGatewayFlags flags) 
{
   return GetGateway()->TreeGateway_PingServer(this, tag, flags);
}

bool ITreeGatewaySubscriber :: IsTreeGatewayConnected() const 
{
   return GetGateway()->TreeGateway_IsGatewayConnected();
}

};

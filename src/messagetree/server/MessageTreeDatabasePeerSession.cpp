#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"

namespace zg
{

MessageTreeDatabasePeerSession :: MessageTreeDatabasePeerSession(const ZGPeerSettings & zgPeerSettings) : ZGDatabasePeerSession(zgPeerSettings), ProxyTreeGateway(NULL), _muxGateway(this)
{
   // empty
}

void MessageTreeDatabasePeerSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   const bool wasFullyAttached = IAmFullyAttached();
   ZGDatabasePeerSession::PeerHasComeOnline(peerID, peerInfo);
   if ((wasFullyAttached == false)&&(IAmFullyAttached())) ProxyTreeGateway::TreeGatewayConnectionStateChanged();  // notify our subscribers that we're now connected to the database.
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG Add [%s]\n", subscriptionPath());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG Remove [%s]\n", subscriptionPath());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags)
{
printf("ZG RemoveAll\n");
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG Request [%s]\n", queryString());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
printf("ZG RequestSubtrees [%s]\n", queryStrings.HeadWithDefault()());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
printf("ZG UploadNodeValue [%s] %p\n", path(), optPayload());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
printf("ZG UploadNodeSubtree [%s]\n", basePath());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG RequestDeleteNodes [%s]\n", path());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, TreeGatewayFlags flags)
{
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags)
{
printf("ZG PingServer [%s]\n", tag());
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_TOSENIOR))
   {
      return B_UNIMPLEMENTED;  // todo implement this!
   }
   else 
   {
      TreeServerPonged(tag);
      return B_NO_ERROR;
   }
}

};  // end namespace zg

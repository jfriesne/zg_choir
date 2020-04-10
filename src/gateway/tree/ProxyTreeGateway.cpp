#include "zg/gateway/tree/ProxyTreeGateway.h"

namespace zg {

ProxyTreeGateway :: ProxyTreeGateway(ITreeGateway * optUpstreamGateway)
   : ITreeGatewaySubscriber(optUpstreamGateway)
{
   // empty
}

ProxyTreeGateway :: ~ProxyTreeGateway()
{
   // empty
}

void ProxyTreeGateway :: ShutdownGateway()
{
   GetGateway()->ShutdownGateway();  // tell our upstream gateway to shut down also
   ITreeGateway::ShutdownGateway();
}

void ProxyTreeGateway :: CommandBatchBegins()
{
   ITreeGatewaySubscriber::BeginCommandBatch();
}

void ProxyTreeGateway :: CommandBatchEnds()
{
   ITreeGatewaySubscriber::EndCommandBatch();
}

status_t ProxyTreeGateway :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::AddTreeSubscription(subscriptionPath, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RemoveTreeSubscription(subscriptionPath, optFilterRef);
}

status_t ProxyTreeGateway :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * /*calledBy*/)
{
   return ITreeGatewaySubscriber::RemoveAllTreeSubscriptions();
}

status_t ProxyTreeGateway :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestTreeNodeValues(queryString, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * /*calledBy*/, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestTreeNodeSubtrees(queryStrings, queryFilters, tag, maxDepth, flags);
}

status_t ProxyTreeGateway :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
   return ITreeGatewaySubscriber::UploadTreeNodeValue(path, optPayload, flags, optBefore);
}

status_t ProxyTreeGateway :: TreeGateway_UploadNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::UploadTreeNodeValues(basePath, valuesMsg, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestDeleteTreeNodes(path, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const char * optBefore, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestMoveTreeIndexEntry(path, optBefore, flags);
}

status_t ProxyTreeGateway :: TreeGateway_PingServer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::PingTreeServer(tag, flags);
}

bool ProxyTreeGateway :: TreeGateway_IsGatewayConnected() const
{
   return ITreeGatewaySubscriber::IsTreeGatewayConnected();
}

// Begin ITreeGatewaySubscriber callback API

void ProxyTreeGateway :: TreeCallbackBatchBeginning()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeCallbackBatchBeginning();
}

void ProxyTreeGateway :: TreeCallbackBatchEnding()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeCallbackBatchEnding();
}

void ProxyTreeGateway :: TreeNodeUpdated(const String & nodePath, const MessageRef & nodeMsg)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeUpdated(nodePath, nodeMsg);
}

void ProxyTreeGateway :: TreeNodeIndexCleared(const String & nodePath)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeIndexCleared(nodePath);
}

void ProxyTreeGateway :: TreeNodeIndexEntryInserted(const String & nodePath, uint32 insertedAtIndex, const String & nodeName)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeIndexEntryInserted(nodePath, insertedAtIndex, nodeName);
}

void ProxyTreeGateway :: TreeNodeIndexEntryRemoved(const String & nodePath, uint32 removedAtIndex, const String & nodeName)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeIndexEntryRemoved(nodePath, removedAtIndex, nodeName);
}

void ProxyTreeGateway :: TreeServerPonged(const String & tag)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeServerPonged(tag);
}

void ProxyTreeGateway :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->SubtreesRequestResultReturned(tag, subtreeData);
}

void ProxyTreeGateway :: TreeGatewayConnectionStateChanged()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeGatewayConnectionStateChanged();
}

void ProxyTreeGateway :: TreeGatewayShuttingDown()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeGatewayShuttingDown();
}

};

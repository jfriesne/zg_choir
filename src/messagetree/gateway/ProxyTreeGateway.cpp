#include "zg/messagetree/gateway/ProxyTreeGateway.h"
#include "zg/messagetree/gateway/DummyTreeGateway.h"

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
   if (GetGateway()) GetGateway()->ShutdownGateway();  // tell our upstream gateway to shut down also
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
   return ITreeGatewaySubscriber::RemoveTreeSubscription(subscriptionPath, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * /*calledBy*/, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RemoveAllTreeSubscriptions(flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestTreeNodeValues(queryString, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * /*calledBy*/, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestTreeNodeSubtrees(queryStrings, queryFilters, tag, maxDepth, flags);
}

status_t ProxyTreeGateway :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore)
{
   return ITreeGatewaySubscriber::UploadTreeNodeValue(path, optPayload, flags, optBefore);
}

status_t ProxyTreeGateway :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * /*calledBy*/, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::UploadTreeNodeSubtree(basePath, valuesMsg, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestDeleteTreeNodes(path, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const String * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::RequestMoveTreeIndexEntry(path, optBefore, optFilterRef, flags);
}

status_t ProxyTreeGateway :: TreeGateway_PingServer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::PingTreeServer(tag, flags);
}

status_t ProxyTreeGateway :: TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, uint32 whichDB, TreeGatewayFlags flags)
{
   return ITreeGatewaySubscriber::PingTreeSeniorPeer(tag, whichDB, flags);
}

status_t ProxyTreeGateway :: TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return ITreeGatewaySubscriber::BeginUndoSequence(optSequenceLabel, whichDB);
}

status_t ProxyTreeGateway :: TreeGateway_EndUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return ITreeGatewaySubscriber::EndUndoSequence(optSequenceLabel, whichDB);
}

status_t ProxyTreeGateway :: TreeGateway_RequestUndo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB)
{
   return ITreeGatewaySubscriber::RequestUndo(whichDB);
}

status_t ProxyTreeGateway :: TreeGateway_RequestRedo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB)
{
   return ITreeGatewaySubscriber::RequestUndo(whichDB);
}

bool ProxyTreeGateway :: TreeGateway_IsGatewayConnected() const
{
   return ITreeGatewaySubscriber::IsTreeGatewayConnected();
}

// Begin ITreeGatewaySubscriber callback API

void ProxyTreeGateway :: CallbackBatchBegins()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) CallBeginCallbackBatch(iter.GetKey());
}

void ProxyTreeGateway :: CallbackBatchEnds()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) CallEndCallbackBatch(iter.GetKey());
}

void ProxyTreeGateway :: TreeNodeUpdated(const String & nodePath, const MessageRef & nodeMsg)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeUpdated(nodePath, nodeMsg);
}

void ProxyTreeGateway :: TreeNodeIndexCleared(const String & nodePath)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeIndexCleared(nodePath);
}

void ProxyTreeGateway :: TreeNodeIndexEntryInserted(const String & nodePath, uint32 insertedAtIndex, const String & nodeName)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeIndexEntryInserted(nodePath, insertedAtIndex, nodeName);
}

void ProxyTreeGateway :: TreeNodeIndexEntryRemoved(const String & nodePath, uint32 removedAtIndex, const String & nodeName)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeNodeIndexEntryRemoved(nodePath, removedAtIndex, nodeName);
}

void ProxyTreeGateway :: TreeServerPonged(const String & tag)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeServerPonged(tag);
}

void ProxyTreeGateway :: TreeSeniorPeerPonged(const String & tag, uint32 whichDB)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeSeniorPeerPonged(tag, whichDB);
}

void ProxyTreeGateway :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->SubtreesRequestResultReturned(tag, subtreeData);
}

void ProxyTreeGateway :: TreeGatewayConnectionStateChanged()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeGatewayConnectionStateChanged();
}

void ProxyTreeGateway :: TreeGatewayShuttingDown()
{
   for (HashtableIterator<ITreeGatewaySubscriber *, uint32> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) iter.GetKey()->TreeGatewayShuttingDown();
}

};

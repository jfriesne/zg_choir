#ifndef ProxyTreeGateway_h
#define ProxyTreeGateway_h

#include "zg/messagetree/gateway/ITreeGateway.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

namespace zg {

/** This class just forwards on all requests from its downstream ITreeGatewaySubscribers to its upstream ITreeGateway,
  * and all replies from its upstream ITreeGateway back to its downstream ITreeGatewaySubscribers.
  * It's not that useful on its own, but rather is generally used as a starting point to subclass from.
  */
class ProxyTreeGateway : public ITreeGateway, public ITreeGatewaySubscriber
{
public:
   /** Constructor
     * @param optUpstreamGateway if non-NULL, this is a pointer to the "upstream" gateway that we will pass our subscribers' request on to, and receive replies back from
     */
   ProxyTreeGateway(ITreeGateway * optUpstreamGateway);

   /** Destructor */
   virtual ~ProxyTreeGateway();

   virtual void ShutdownGateway();

public:
   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg, const String & optOpTag);
   virtual void TreeNodeIndexCleared(const String & path, const String & optOpTag);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeLocalPeerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void MessageReceivedFromTreeSeniorPeer(int32 optWhichDB, const String & tag, const MessageRef & payload);
   virtual void MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeGatewayShuttingDown();

protected:
   // IGateway function-call API
   virtual void CommandBatchBegins();
   virtual void CommandBatchEnds();

   // IGatewaySubscriber callback API
   virtual void CallbackBatchBegins();
   virtual void CallbackBatchEnds();

   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & tag);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag);
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags, const String & optOpTag);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const String & optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag);
   virtual status_t TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags);
   virtual status_t TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * calledBy, const MessageRef & msg, uint32 whichDB, const String & tag);
   virtual status_t TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * calledBy, const String & subscriberPath, const MessageRef & msg, const ConstQueryFilterRef & optFilterRef, const String & tag);
   virtual status_t TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB);
   virtual status_t TreeGateway_EndUndoSequence(  ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB);
   virtual status_t TreeGateway_RequestUndo(ITreeGatewaySubscriber * calledBy, uint32 whichDB, const String & optOpTag);
   virtual status_t TreeGateway_RequestRedo(ITreeGatewaySubscriber * calledBy, uint32 whichDB, const String & optOpTag);
   virtual bool TreeGateway_IsGatewayConnected() const;
   virtual ConstMessageRef TreeGateway_GetGestaltMessage() const;
};

};  // end namespace zg

#endif

#ifndef MuxTreeGateway_h
#define MuxTreeGateway_h

#include "zg/messagetree/gateway/ProxyTreeGateway.h"
#include "reflector/DataNode.h"
#include "regex/PathMatcher.h"
#include "util/Hashtable.h"
#include "util/TimeUtilityFunctions.h"

namespace zg {

class ITreeGatewaySubscriber;
class MessageTreeDatabasePeerSession;

/** This class contains the logic to multiplex the requests of multiple ITreeGatewaySubscriber objects
  * into a single request-stream for an upstream ITreeGateway to handle, and to demultiplex the results
  * coming back from the upstream ITreeGateway for our own ITreeGatewaySubscriber objects.
  */
class MuxTreeGateway : public ProxyTreeGateway
{
public:
   /** Constructor
     * @param optUpstreamGateway if non-NULL, this is a pointer to the "upstream" gateway that we will pass our subscribers' request on to, and receive replies back from
     */
   MuxTreeGateway(ITreeGateway * optUpstreamGateway);

   /** Destructor */
   virtual ~MuxTreeGateway();

   virtual void ShutdownGateway();

   virtual void CallbackBatchBegins();
   virtual void CallbackBatchEnds();

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & tag);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const ConstMessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag);
   virtual status_t TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags);
   virtual status_t TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * calledBy, const ConstMessageRef & msg, uint32 whichDB, const String & tag);
   virtual status_t TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * calledBy, const String & subscriberPath, const ConstMessageRef & msg, const ConstQueryFilterRef & optFilterRef, const String & tag);

   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const ConstMessageRef & payloadMsg, const String & optOpTag);
   virtual void TreeNodeIndexCleared(const String & path, const String & optOpTag);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeLocalPeerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void MessageReceivedFromTreeSeniorPeer(int32 optWhichDB, const String & tag, const MessageRef & payload);
   virtual void MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();

   // IGateway internal API
   virtual void RegisterSubscriber(void * s);
   virtual void UnregisterSubscriber(void * s);

   /** Called when one of our subscribers calls RemoveSubscription() or RemoveAllSubscriptions().
     * @param calledBy the subscriber who called RemoveSubscription() or RemoveAllSubscriptions()
     * @param receivedPath the path to a node that this subscriber had received updates about, but no longer will
     * Default implementation is a no-op.  This method is here primarily so that the SymlinkLogicMuxTreeGateway subclass
     * can override it to remove symlink-subscriptions that are no longer necessary.
     */
   virtual void ReceivedPathDropped(ITreeGatewaySubscriber * calledBy, const String & receivedPath);

   /** Returns true if at least one of our current subscribers has a subscription that matches the specified path.
     * @param receivedPath a node path that we want to know if anyone is subscribed to
     */
   MUSCLE_NODISCARD bool IsAnyoneSubscribedToPath(const String & receivedPath) const;

   /** Returns the set of ITreeGatewaySubscribers that we are currently receiving initial-subscription-results for
     * (ie results for nodes that were already on the server when these subscribers called AddTreeSubscription()
     */
   MUSCLE_NODISCARD const Hashtable<ITreeGatewaySubscriber *, Void> & GetSubscribersInInitialResultsMode() const {return _allowedCallbacks;}

private:
   friend class MessageTreeDatabasePeerSession;

   class TreeSubscriberInfo : public PathMatcher
   {
   public:
      TreeSubscriberInfo() {/* empty */}
      Hashtable<String, uint32> _receivedPaths;  // node-path -> number-of-segments-in-path
   };
   DECLARE_REFTYPES(TreeSubscriberInfo);

   status_t TreeGateway_RequestNodeSubtreesAux(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags, char markerChar);

   status_t UpdateSubscription(const String & subscriptionPath, ITreeGatewaySubscriber * optSubscriber, TreeGatewayFlags flags);
   void UpdateSubscriber(ITreeGatewaySubscriber * sub, TreeSubscriberInfo & subInfo, const String & path, const ConstMessageRef & msgRef, const String & optOpTag);
   void TreeNodeUpdatedAux(const String & path, const ConstMessageRef & msgRef, const String & optOpTag, ITreeGatewaySubscriber * optDontNotify);
   MUSCLE_NODISCARD bool DoesPathMatch(ITreeGatewaySubscriber * sub, const PathMatcher * pm, const String & path, const Message * optMessage) const;
   void EnsureSubscriberInBatchGroup(ITreeGatewaySubscriber * sub);
   void DoIndexNotifications(const String & path, char opCode, uint32 index, const String & nodeName, const String & optOpTag);
   void DoIndexNotificationAux(ITreeGatewaySubscriber * sub, const String & path, char opCode, uint32 index, const String & nodeName, const String & optOpTag);

   MUSCLE_NODISCARD String GetRegistrationIDPrefix(ITreeGatewaySubscriber * sub, char markerChar='_') const;
   MUSCLE_NODISCARD String PrependRegistrationIDPrefix(ITreeGatewaySubscriber * sub, const String & s, char markerChar='_') const;
   MUSCLE_NODISCARD String TagToExcludeClientFromReplies(ITreeGatewaySubscriber * excludeMe, const String & tag) const;

   ITreeGatewaySubscriber * ParseRegistrationID(const String & ascii, char markerChar='_') const;
   ITreeGatewaySubscriber * ParseRegistrationIDPrefix(const String & s, String & retSuffix, char markerChar='_') const;

   const String _muxTreeGatewayIDPrefix;

   Hashtable<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> _subscriberInfos;
   Hashtable<ITreeGatewaySubscriber *, Void> _needsCallbackBatchEndsCall;
   Hashtable<ITreeGatewaySubscriber *, Queue<String> > _requestedSubtrees;

   class SubscriptionInfo
   {
   public:
      SubscriptionInfo() : _sub(NULL) {/* empty */}
      SubscriptionInfo(ITreeGatewaySubscriber * sub, const ConstQueryFilterRef & qfRef, TreeGatewayFlags flags) : _sub(sub), _qfRef(qfRef), _flags(flags) {/* empty */}

      bool operator == (const SubscriptionInfo & rhs) const {return ((_sub == rhs._sub)&&(_qfRef == rhs._qfRef)&&(_flags == rhs._flags));}
      bool operator != (const SubscriptionInfo & rhs) const {return !(*this==rhs);}

      ITreeGatewaySubscriber * GetSubscriber() const {return _sub;}
      const ConstQueryFilterRef & GetQueryFilter() const {return _qfRef;}
      TreeGatewayFlags GetFlags() const {return _flags;}

   private:
      ITreeGatewaySubscriber * _sub;
      ConstQueryFilterRef _qfRef;
      TreeGatewayFlags _flags;
   };
   Hashtable<String, Queue<SubscriptionInfo> > _subscribedStrings;

   bool _isConnected;
   Hashtable<ITreeGatewaySubscriber *, Void> _allowedCallbacks;  // table of subscribers that are in their receiving-initial-results period

   mutable DataNode _dummyNode;  // just to support NodeNameQueryFilter

   ITreeGatewaySubscriber _dummySubscriber;  // here solely so that we can place it into (_allowedCallbacks) if necessary
};

};  // end namespace zg

#endif

#ifndef MuxTreeGateway_h
#define MuxTreeGateway_h

#include "zg/messagetree/gateway/ProxyTreeGateway.h"
#include "regex/PathMatcher.h"
#include "util/Hashtable.h"

namespace zg {

class ITreeGatewaySubscriber;

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
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore);
   virtual status_t TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags);
   virtual status_t TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * calledBy, const MessageRef & msg, uint32 whichDB, const String & tag);
   virtual status_t TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * calledBy, const String & subscriberPath, const MessageRef & msg, const String & tag);
   
   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeLocalPeerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void MessageReceivedFromTreeSeniorPeer(int32 optWhichDB, const String & tag, const MessageRef & payload);
   virtual void MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();

   // IGateway internal API
   virtual void RegisterSubscriber(void * s);
   virtual void UnregisterSubscriber(void * s);
  
private:
   class TreeSubscriberInfo : public PathMatcher
   {
   public:
      TreeSubscriberInfo() {/* empty */}
      Hashtable<String, uint32> _receivedPaths;  // node-path -> number-of-segments-in-path
   };
   DECLARE_REFTYPES(TreeSubscriberInfo);

   status_t UpdateSubscription(const String & subscriptionPath, ITreeGatewaySubscriber * optSubscriber, TreeGatewayFlags flags);
   void UpdateSubscriber(ITreeGatewaySubscriber * sub, TreeSubscriberInfo & subInfo, const String & path, const MessageRef & msgRef);
   void TreeNodeUpdatedAux(const String & path, const MessageRef & msgRef, ITreeGatewaySubscriber * optDontNotify);
   bool DoesPathMatch(ITreeGatewaySubscriber * sub, const PathMatcher * pm, const String & path, const Message * optMessage) const;
   void EnsureSubscriberInBatchGroup(ITreeGatewaySubscriber * sub);
   void DoIndexNotifications(const String & path, char opCode, uint32 index, const String & nodeName);
   void DoIndexNotificationAux(ITreeGatewaySubscriber * sub, const String & path, char opCode, uint32 index, const String & nodeName);

   String GetRegistrationIDPrefix(ITreeGatewaySubscriber * sub) const;
   String PrependRegistrationIDPrefix(ITreeGatewaySubscriber * sub, const String & s) const;
   ITreeGatewaySubscriber * ParseRegistrationID(const String & ascii) const;
   ITreeGatewaySubscriber * ParseRegistrationIDPrefix(const String & s, String & retSuffix) const;

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
   Hashtable<ITreeGatewaySubscriber *, Void> _allowedCallbacks;  // untrusted pointers, do not dereference
};

};  // end namespace zg

#endif

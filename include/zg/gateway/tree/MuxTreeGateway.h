#ifndef MuxTreeGateway_h
#define MuxTreeGateway_h

#include "zg/gateway/tree/ProxyTreeGateway.h"
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
   MuxTreeGateway(ITreeGateway * optUpstreamGateway);
   virtual ~MuxTreeGateway();

   virtual void ShutdownGateway();

   virtual void CallbackBatchBegins();
   virtual void CallbackBatchEnds();

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy); 
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore);
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   
   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeServerPonged(const String & tag);
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
      Hashtable<String, Void> _receivedPaths;
   };
   DECLARE_REFTYPES(TreeSubscriberInfo);

   status_t UpdateSubscription(const String & subscriptionPath, ITreeGatewaySubscriber * optSubscriber, TreeGatewayFlags flags);
   void UpdateSubscriber(ITreeGatewaySubscriber * sub, TreeSubscriberInfo & subInfo, const String & path, const MessageRef & msgRef);
   void TreeNodeUpdatedAux(const String & path, const MessageRef & msgRef, ITreeGatewaySubscriber * optDontNotify);
   bool DoesPathMatch(ITreeGatewaySubscriber * sub, const PathMatcher * pm, const String & path, const Message * optMessage) const;
   void EnsureSubscriberInBatchGroup(ITreeGatewaySubscriber * sub);
   void DoIndexNotifications(const String & path, char opCode, uint32 index, const String & nodeName);
   void DoIndexNotificationAux(ITreeGatewaySubscriber * sub, const String & path, char opCode, uint32 index, const String & nodeName);

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

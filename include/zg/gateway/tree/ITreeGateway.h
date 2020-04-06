#ifndef ITreeGateway_h
#define ITreeGateway_h

#include "zg/gateway/IGateway.h"
#include "message/Message.h"
#include "support/BitChord.h"
#include "regex/QueryFilter.h"
#include "util/String.h"

namespace zg {

class ITreeGatewaySubscriber;

/** Flags used to optionally modify the behavior of ITreeGateway commands */
enum {
   TREE_GATEWAY_FLAG_INDEXED = 0,  /**< If set, the uploaded node should be added to it's parent's ordered-nodes index. */
   TREE_GATEWAY_FLAG_NOREPLY,      /**< If set, no initial reply is desired */
   TREE_GATEWAY_FLAG_TOSENIOR,     /**< If set in a call to PingTreeServer(), the ping will travel to the senior peer before coming back */
   NUM_TREE_GATEWAY_FLAGS          /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(TreeGatewayFlags, NUM_TREE_GATEWAY_FLAGS);

/** Abstract base class for objects that want to implement the ITreeGateway API */
class ITreeGateway : public IGateway
{
public:
   ITreeGateway() {/* empty */}
   virtual ~ITreeGateway() {/* empty */}

   virtual void ShutdownGateway();

protected:
   virtual status_t AddTreeSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t RemoveTreeSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef) = 0;
   virtual status_t RemoveAllTreeSubscriptions(ITreeGatewaySubscriber * calledBy) = 0;
   virtual status_t RequestTreeNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t RequestTreeNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags) = 0;
   virtual status_t UploadTreeNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore) = 0;
   virtual status_t UploadTreeNodeValues(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags) = 0;
   virtual status_t RequestDeleteTreeNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t RequestMoveTreeIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, TreeGatewayFlags flags) = 0;
   virtual status_t PingTreeServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags) = 0;
   virtual bool IsTreeGatewayConnected() const = 0;

private:
   friend class ITreeGatewaySubscriber;
};

/** Returns a pointer to a singleton DummyTreeGateway object, which is used to cleanly implement no-ops and return errors on all methods */
ITreeGateway * GetDummyTreeGateway();

/** Abstract base class for objects that want to interact with an ITreeGateway's public API. */
class ITreeGatewaySubscriber : public IGatewaySubscriber
{
public:
   ITreeGatewaySubscriber(ITreeGateway * gateway) : IGatewaySubscriber(gateway) {/* empty */}
   virtual ~ITreeGatewaySubscriber() {/* empty */}

   void SetTreeGateway(ITreeGateway * gateway) {IGatewaySubscriber::SetGateway(gateway);}
   ITreeGateway * GetTreeGateway() const {ITreeGateway * gw = static_cast<ITreeGateway *>(GetGateway()); return gw ? gw : GetDummyTreeGateway();}

   // ITreeGateway Command pass-through API
   virtual void BeginTreeCommandBatch();
   virtual void EndTreeCommandBatch();

   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->AddTreeSubscription(this, subscriptionPath, optFilterRef, flags);}
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef) {return GetTreeGateway()->RemoveTreeSubscription(this, subscriptionPath, optFilterRef);}
   virtual status_t RemoveAllTreeSubscriptions() {return GetTreeGateway()->RemoveAllTreeSubscriptions(this);}

   virtual status_t RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->RequestTreeNodeValues(this, queryString, optFilterRef, flags);}
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->RequestTreeNodeSubtrees(this, queryStrings, queryFilters, tag, maxDepth, flags);}

   virtual status_t UploadTreeNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags = TreeGatewayFlags(), const char * optBefore = NULL) {return GetTreeGateway()->UploadTreeNodeValue(this, path, optPayload, flags, optBefore);}
   virtual status_t UploadTreeNodeValues(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->UploadTreeNodeValues(this, basePath, valuesMsg, flags);}

   virtual status_t RequestDeleteTreeNodes(const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->RequestDeleteTreeNodes(this, path, optFilterRef, flags);}
   virtual status_t RequestMoveTreeIndexEntry(const String & path, const char * optBefore, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->RequestMoveTreeIndexEntry(this, path, optBefore, flags);}

   virtual status_t PingTreeServer(const String & tag, TreeGatewayFlags flags = TreeGatewayFlags()) {return GetTreeGateway()->PingTreeServer(this, tag, flags);}

   virtual bool IsTreeGatewayConnected() const {return GetTreeGateway()->IsTreeGatewayConnected();}

protected:
   // ITreeGateway callback API

   virtual void TreeCallbackBatchBeginning() {/* empty */}
   virtual void TreeCallbackBatchEnding() {/* empty */}

   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg) {(void) nodePath; (void) payloadMsg;}
   virtual void TreeNodeIndexCleared(const String & path) {(void) path;}
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName) {(void) path; (void) insertedAtIndex; (void) nodeName;}
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName) {(void) path; (void) removedAtIndex; (void) nodeName;}
   virtual void TreeServerPonged(const String & tag) {(void) tag;}

   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData) {(void) tag; (void) subtreeData;}

   virtual void TreeGatewayConnectionStateChanged() {/* empty */}
   virtual void TreeGatewayShuttingDown() {/* empty */}

private:
   friend class ITreeGateway;
};

/** Dummy subclass of ITreeGateway; everything is a on-op */
class DummyTreeGateway : public ITreeGateway
{
public:
   DummyTreeGateway(status_t returnValue = B_UNIMPLEMENTED) : _returnValue(returnValue) {/* empty */}

protected:
   virtual status_t AddTreeSubscription(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t RemoveTreeSubscription(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &) {return _returnValue;}
   virtual status_t RemoveAllTreeSubscriptions(ITreeGatewaySubscriber *) {return _returnValue;}
   virtual status_t RequestTreeNodeValues(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t RequestTreeNodeSubtrees(ITreeGatewaySubscriber *, const Queue<String> &, const Queue<ConstQueryFilterRef> &, const String &, uint32, TreeGatewayFlags) {return _returnValue;}
   virtual status_t UploadTreeNodeValue(ITreeGatewaySubscriber *, const String &, const MessageRef &, TreeGatewayFlags, const char *) {return _returnValue;}
   virtual status_t UploadTreeNodeValues(ITreeGatewaySubscriber *, const String &, const MessageRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t RequestDeleteTreeNodes(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t RequestMoveTreeIndexEntry(ITreeGatewaySubscriber *, const String &, const char *, TreeGatewayFlags) {return _returnValue;}
   virtual status_t PingTreeServer(ITreeGatewaySubscriber *, const String &, TreeGatewayFlags) {return _returnValue;}

   virtual bool IsTreeGatewayConnected() const {return false;}

private:
   const status_t _returnValue;
};

};  // end namespace zg

#endif

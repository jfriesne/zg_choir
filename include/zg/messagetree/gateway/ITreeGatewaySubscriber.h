#ifndef ITreeGatewaySubscriber_h
#define ITreeGatewaySubscriber_h

#include "support/BitChord.h"
#include "message/Message.h"
#include "regex/QueryFilter.h"
#include "zg/gateway/IGatewaySubscriber.h"

namespace zg {

class ITreeGateway;

/** Flags used to optionally modify the behavior of ITreeGateway commands */
enum {
   TREE_GATEWAY_FLAG_INDEXED = 0,  /**< If set, the uploaded node should be added to it's parent's ordered-nodes index. */
   TREE_GATEWAY_FLAG_NOREPLY,      /**< If set, no initial reply is desired */
   TREE_GATEWAY_FLAG_INTERIM,      /**< If set, this update is considered idempotent and can therefore be skipped when performing an undo or redo operation */
   TREE_GATEWAY_FLAG_SYNC,         /**< If set in PingTreeLocalPeer(), the ping will be pong'd by the client's own network I/O thread rather than going out to the server */
   NUM_TREE_GATEWAY_FLAGS          /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(TreeGatewayFlags, NUM_TREE_GATEWAY_FLAGS);

class ITreeGateway;
class GatewaySubscriberUndoBatchGuard;

/** Abstract base class for objects that want to connect to an ITreeGateway as downstream clients */
class ITreeGatewaySubscriber : public IGatewaySubscriber<ITreeGateway>
{
public:
   /** Constructor
     * @param optGateway pointer to the ITreeGateway this subscriber should register with.  May be NULL
     *                   if you don't want to register right away (in which case you'll probably want
     *                   to call SetGateway() on this object later)
     */
   ITreeGatewaySubscriber(ITreeGateway * optGateway) : IGatewaySubscriber<ITreeGateway>(optGateway) {/* empty */}

   /** Destructor */
   virtual ~ITreeGatewaySubscriber() {/* empty */}

public:
   // ITreeGatewaySubscriber callback API

   /** Called by the upstream gateway to notify this subscriber about the new current state of a particular subscribed-to database node.
     * @param nodePath the session-relative path of the database node in question.
     * @param optPayloadMsg a reference to the node's current payload-Message, or a NULL reference if the node has been deleted.
     * @param optOpTag If the entity responsible for this database change specified an operation-tag for the change, that tag will appear in this argument.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg, const String & optOpTag) {(void) nodePath; (void) optPayloadMsg; (void) optOpTag;}

   /** Called by the upstream gateway to notify this subscriber when the node-index of a subscribed-to database node has been cleared.
     * @param nodePath the session-relative path of the database node in question.
     * @param optOpTag If the entity responsible for this database change specified an operation-tag for the change, that tag will appear in this argument.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeIndexCleared(const String & nodePath, const String & optOpTag) {(void) nodePath; (void) optOpTag;}

   /** Called by the upstream gateway to notify this subscriber when a new entry has been inserted into the node-index of a subscribed-to database node.
     * @param nodePath the session-relative path of the database node in question.
     * @param insertedAtIndex position at which the entry has been inserted (0==first in the index, 1==second in the index, etc)
     * @param nodeName the name of the child node that was inserted at the specified position.
     * @param optOpTag If the entity responsible for this database change specified an operation-tag for the change, that tag will appear in this argument.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeIndexEntryInserted(const String & nodePath, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag) {(void) nodePath; (void) insertedAtIndex; (void) nodeName; (void) optOpTag;}

   /** Called by the upstream gateway to notify this subscriber when an entry has been removed from the node-index of a subscribed-to database node.
     * @param nodePath the session-relative path of the database node in question.
     * @param removedAtIndex position from which the entry has been removed (0==first in the index, 1==second in the index, etc)
     * @param nodeName the name of the child node that was removed from the specified position.
     * @param optOpTag If the entity responsible for this database change specified an operation-tag for the change, that tag will appear in this argument.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeIndexEntryRemoved(const String & nodePath, uint32 removedAtIndex, const String & nodeName, const String & optOpTag) {(void) nodePath; (void) removedAtIndex; (void) nodeName; (void) optOpTag;}

   /** Called when a "pong" comes back from the server this client is directly connected to (in response to a previous call to PingTreeLocalPeer()).
     * @param tag the tag-string that you had previously passed to PingTreeLocalPeer().
     * Default implementation is a no-op.
     */
   virtual void TreeLocalPeerPonged(const String & tag) {(void) tag;}

   /** Called when a "pong" comes back from the senior peer (in response to a previous call to PingTreeSeniorPeer()).
     * @param tag the tag-string that was previously passed to PingTreeSeniorPeer().
     * @param whichDB index of the ZG database whose database-update mechanisms the senior-peer-ping had travelled through.
     * Default implementation is a no-op.
     */
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB) {(void) tag; (void) whichDB;}

   /** Called when a Message has been received from the Senior Peer (typically in response to an earlier call by this object to SendMessageToTreeSeniorPeer())
     * @param optWhichDB index of the database the reply is coming from, or -1 if it is coming the the MessageTreePeerSession itself.
     * @param tag the tag-String that we previously passed in to SendMessageToTreeSeniorPeer()
     * @param payload the payload Message that the senior peer is sending to us.
     * Default implementation is a no-op.
     */
   virtual void MessageReceivedFromTreeSeniorPeer(int32 optWhichDB, const String & tag, const MessageRef & payload) {(void) optWhichDB; (void) tag; (void) payload;}

   /** Called when a Message has been received from another ITreeGatewaySubscriber.
     * @param nodePath Node-path of a node that this ITreeGatewaySubscriber is subscribed to, that was matched by the sender
     *                 of this Message as a way to route the Message to this ITreeGatewaySubscriber.  May be empty if this
     *                 Message was sent to use via our return-address string rather than via node-path-matching.
     * @param payload the payload Message that the senior peer is sending to us.
     * @param returnAddress A String that can be passed to SendMessageToSubscriber()'s (subscriberPath) argument instead of
     *                      a node-path string, if you just want to reply directly to the sender of this Message.
     * Default implementation is a no-op.
     */
   virtual void MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress) {(void) nodePath; (void) payload; (void) returnAddress;}

   /** Called when the subtree-data Message comes back in response to a previous call to RequestTreeNodeSubtrees().
     * @param tag the tag-string that you had previously passed to RequestTreeNodeSubtrees()
     * @param subtreeData a Message containing the subtree data, or a NULL Message if the query failed for some readon.
     * Default implementation is a no-op.
     */
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData) {(void) tag; (void) subtreeData;}

   /** Called when the connection between our gateway and its upstream database has been severed, or has become usable again. 
     * Default implementation is a no-op.  (You can call IsTreeGatewayConnected() to see what the current state is)
     */ 
   virtual void TreeGatewayConnectionStateChanged() {/* empty */}

   /** Called just before our gateway gets destroyed -- this is your last chance to talk to the gateway before its demise.
     * Default implementation is a no-op.
     */ 
   virtual void TreeGatewayShuttingDown() {/* empty */}

protected:
   // Function-call API -- passed through to our ITreeGateway's function-call API

   /** Call this to subscribe to a database node (or a wildcarded-set of database nodes) on the server, so that you will
     * be notified of their current state and whenever they change in the future.
     * @param subscriptionPath the session-relative path of the node(s) you wish to subscribe to (e.g. "foo/bar/ba*")
     * @param optFilterRef if non-NULL, a reference to a QueryFilter object that the server should use to limit which nodes match the subscription.
     * @param flags If specified, these flags can influence the behavior of the subscribe operation.  Currently only the TREE_GATEWAY_FLAG_NOREPLY
     *              flag has an effect here; if specified, the initial/current state of the matching data nodes will not be send back to the
     *              subscriber (i.e. only future updates to the nodes will cause TreeNodeUpdated() to be called)
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());

   /** Call this to cancel a subscription to a database node (or a wildcarded-set of database nodes) on the server, so that you will
     * no longer be notified of their current state and whenever they change in the future.
     * @param subscriptionPath the session-relative path of the node(s) you wish to unsubscribe from.  This should be the same
     *                         as a subscription-path string you previously passed to AddTreeSubscription().
     * @param optFilterRef if non-NULL, a reference to a QueryFilter object that you previously passed to AddTreeSubscription()
     *                     at the same time you passed (subscriptionPath) to AddTreeSubscription().
     * @param flags If specified, these flags can influence the behavior of the subscribe operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());

   /** Call this if you want to cancel ALL subscriptions your ITreeGatewaySubscriber currently has registered with the gateway.
     * @param flags If specified, these flags can influence the behavior of the subscribe operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RemoveAllTreeSubscriptions(TreeGatewayFlags flags = TreeGatewayFlags());

   /** Call this to request a one-shot (i.e. non-persistent) notification of the current state of database nodes matching the specified path.
     * @param queryString the session-relative path of the node(s) you wish to query (e.g. "foo/bar/ba*")
     * @param optFilterRef if non-NULL, a reference to a QueryFilter object that the server should use to limit which nodes match the query.
     * @param flags If specified, these flags can influence the behavior of the subscribe operation.  Currently this argument is ignored.
     * @note notifications in response to this query will come in the form of some future calls to TreeNodeUpdated().
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());

   /** Call this to request a one-shot (i.e. non-persistent) download of the contents of one or more subtrees of the database.
     * @param queryStrings a set of one or more session-relative paths of the node(s) you wish to download (along with the subtrees of nodes beneath them)
     * @param queryFilters if non-NULL, QueryFilters in this list will be used to limit the nodes selected for download.  The nth (queryFilter) will be applied to the (nth) queryString.
     * @param tag an arbitrary string that can be used to identify this download.  It will be passed back to you, verbatim, in the corresponding SubtreesRequestResultReturned() call.
     * @param maxDepth The maximum depth of the subtrees to return.  Defaults to MUSCLE_NO_LIMIT, which will result in the full subtree being returned, regardless of depth.
     * @param flags If specified, these flags can influence the behavior of the subscribe operation.  Currently this argument is ignored.
     * @note notifications in response to this query will come in the form of some future calls to TreeNodeUpdated().
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth = MUSCLE_NO_LIMIT, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Request that the given specified payload be uploaded to the database.
     * @param nodePath the session-relative path of the node you wish to upload (e.g. "foo/bar/baz").  May be wildcarded only if (optPayload) is NULL.
     *                 If this path ends with a slash, then the server will choose a unique name for the uploaded node.
     * @param optPayload the payload Message to upload, or a NULL reference if you wish to delete one or more nodes instead.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently TREE_GATEWAY_FLAG_INDEXED will cause the
     *              uploaded node to be added to its parent's ordered-children index, and TREE_GATEWAY_FLAG_NOREPLY will prevent any notifications about
     *              this upload from being delivered to any clients (including the caller).
     * @param optBefore Specifies where in the parent's node-index this node should be inserted.  Only used if TREE_GATEWAY_FLAG_INDEXED was specified in the (flags)
     *                  argument.  If non-empty, the node will be inserted directly before the child node with the specified name; or if empty, the node will be
     *                  inserted at the end of the index.  Defaults to an empty string.
     * @param optOpTag An optional string to associate with this operation.  It can be anything you like; it will be passed on verbatim to the TreeNodeUpdated()
     *                 callbacks that subscribed ITreeGatewaySubscriber objects receive as a result of this operation.  Defaults to an empty string.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t UploadTreeNodeValue(const String & nodePath, const MessageRef & optPayload, TreeGatewayFlags flags = TreeGatewayFlags(), const String & optBefore = GetEmptyString(), const String & optOpTag = GetEmptyString());

   /** Request that a subtree of nodes be uploaded to the specified location in the database.
     * @param basePath the session-relative path indicating where the root of the subtree should be created.
     * @param valuesMsg a Message containing the contents of the subtree (as previously passed back to you in SubtreesRequestResultReturned())
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @param optOpTag An optional string to associate with this operation.  It can be anything you like; it will be passed on verbatim to the TreeNodeUpdated()
     *                 callbacks that subscribed ITreeGatewaySubscriber objects receive as a result of this operation.  Defaults to an empty string.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t UploadTreeNodeSubtree(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags = TreeGatewayFlags(), const String & optOpTag = GetEmptyString());

   /** Request that one of more nodes be deleted from the database.
     * @param nodePath Session-relative path to the node (or nodes) to delete.  May be wildcarded.
     * @param optFilterRef if non-NULL, reference to a QueryFilter object that the server will use to limit which nodes get deleted.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @param optOpTag An optional string to associate with this operation.  It can be anything you like; it will be passed on verbatim to the TreeNodeUpdated()
     *                 callbacks that subscribed ITreeGatewaySubscriber objects receive as a result of this operation.  Defaults to an empty string.
     * @note that any children/grandchildren/etc of deleted nodes will also be deleted.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestDeleteTreeNodes(const String & nodePath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags(), const String & optOpTag = GetEmptyString());

   /** Request that an entry within the index of parent-node of one or more specified nodes be moved to a different location in its index.
     * @param nodePath Session-relative path to the node (or nodes) to move within their parents' node-indices.  May be wildcarded (although I'm not sure how useful that is).
     * @param optBefore Specifies where in the parent's node-index the node should be moved to.
     * @param optFilterRef if non-NULL, reference to a QueryFilter object that the server will use to limit which nodes get moved.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @param optOpTag An optional string to associate with this operation.  It can be anything you like; it will be passed on verbatim to the TreeNodeUpdated()
     *                 callbacks that subscribed ITreeGatewaySubscriber objects receive as a result of this operation.  Defaults to an empty string.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestMoveTreeIndexEntry(const String & nodePath, const String & optBefore = GetEmptyString(), const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags(), const String & optOpTag = GetEmptyString());

   /** Sends a "Ping" message to the server this client is directly connected to.
     * @param tag an arbitrary string to send with the ping-message.  Will be sent back verbatim in the corresponding TreeLocalPeerPonged() callback.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t PingTreeLocalPeer(const String & tag, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Sends a "Ping" message to the senior peer.  This Ping message will go through the entire ZG-database-update meat-grinder before 
     * coming back to you in the form of a call to TreeSeniorPeerPonged(), which is useful if you are trying to synchronize your actions
     * with respect to how a database is being updated system-wide.
     * @param tag an arbitrary string to send with the ping-message.  Will be sent back verbatim in the corresponding SeniorPeerPonged() callback.
     * @param whichDB which ZG database index to send the ping Message through.  Defaults to 0.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t PingTreeSeniorPeer(const String & tag, uint32 whichDB = 0, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Sends a user-specified Message to the senior peer.  This Message will result in a call to the MessageReceivedFromSubscriber()
     * callback-method the MessageTreeDatabasePeerSession object on the senior peer.  The default implementation of
     * MessageReceivedFromSubscriber() will then forward the call to the appropriate MessageTreeDatabaseObject, based on (whichDB)
     * @param msg the Message to send.
     * @param whichDB which ZG database index to forward Message to.  Defaults to 0.
     * @param tag optional arbitrary tag-string that will be passed back to any corresponding MessageReceivedFromTreeSeniorPeer() callbacks
     *            that are later called on this object, if/when the senior peer replies to your Message.  Defaults to an empty String.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     * @note the default zg_choir server-side code doesn't do anything useful with the Messages it receives this way, so this
     *       functionality is useful only when you've overridden MessageReceivedFromSubscriber() in a subclass on the server,
     *       with some code that reacts appropriately to the incoming Messages.
     */
   virtual status_t SendMessageToTreeSeniorPeer(const MessageRef & msg, uint32 whichDB = 0, const String & tag = GetEmptyString());

   /** Sends a user-specified Message to one or more other ITreeGatewaySubscriber objects in the system.
     * @param subscriberPath a string specifying which subscriber(s) to send (msg) to.  This String can either be a node-path
     *                      (e.g. "clients/some_peer_id/foo/bar"), or a subscriber-return-address (as was passed to you by a previous
     *                      call to MessageReceivedFromSubscriber().  In the former case, your (msg) will be sent to all
     *                      ITreeGatewaySubscribers that are currently subscribed to at least one of the nodes matched by the path 
     *                      (wildcards in the path are okay).  In the latter case, your (msg) will be sent to the ITreeGatewaySubscriber 
     *                      identified by the subscriber-return-address.
     * @param msg the Message to send to one or more other ITreeGatewaySubscribers.
     * @param optFilterRef an optional reference to a QueryFilter object to use to filter which nodes will be matched by (subscriberPath).
     *                     If non-NULL, only nodes whose payloads match the QueryFilter's criteria will be considered when deciding whom
     *                     to forward (msg) to.  Note that this argument is ignored if (subscriberPath) is a return-address String.
     * @param returnAddress Optional string to form part of the return-address that will be passed to the receivers of the Message.
     *                      In general you want to just leave this at its default value; it is here primarily to support routing during 
     *                      intermediate stages of the Message-sending process.
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note as an optimization, node-paths passed in to the (subscriberPath) argument that match only nodes that within 
     *       one or more peer-specific subtrees (as defined by a ClientDataMessageTreeDatabaseObject) will result in (msg)
     *       being forwarded only to subscribers on the peers matching those nodes.
     */
   virtual status_t SendMessageToSubscriber(const String & subscriberPath, const MessageRef & msg, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), const String & returnAddress = GetEmptyString());

   /** Tells the database that an undoable sequence of changes is about to be uploaded.
     * @param optSequenceLabel A user-readable string describing what the sequence does.  If you don't have a good string to supply
     *                         yet, you can pass an empty String here and specify a string later on in your EndUndoSequence() call.
     * @param whichDB index of the database the undo-sequence should be uploaded into.  This database must be implemented via a 
     *                UndoStackMessageTreeDatabaseObject.  Defaults to zero.
     */
   virtual status_t BeginUndoSequence(const String & optSequenceLabel = GetEmptyString(), uint32 whichDB = 0);

   /** Tells the database that an undoable sequence of changes has been completely uploaded.
     * @param optSequenceLabel An optional user-readable label describing the undo-operation that was completed.
     *                         If non-empty, this string will be used instead of the string that was earlier passed to BeginUndoSequence().
     * @param whichDB index of the database the undo-sequence was uploaded into.  This database must be implemented via a 
     *                UndoStackMessageTreeDatabaseObject.  Defaults to zero.
     * @note this method must be called exactly once for each call to BeginUndoSequence().
     */
   virtual status_t EndUndoSequence(const String & optSequenceLabel = GetEmptyString(), uint32 whichDB = 0);

   /** Request an "undo" of the most recent previously uploaded database change.
     * @param whichDB index of the database to which should perform the undo.  This database must be implemented via a UndoStackMessageTreeDatabaseObject,
     *                or the undo request will be ignored.  Defaults to zero.
     * @param optOpTag An optional string to associate with this operation.  It can be anything you like; it will be passed on verbatim to the TreeNodeUpdated()
     *                 callbacks that subscribed ITreeGatewaySubscriber objects receive as a result of this operation.  Defaults to an empty string.
     */
   virtual status_t RequestUndo(uint32 whichDB = 0, const String & optOpTag = GetEmptyString());

   /** Request a "redo" of a previously un-dnoe database change.
     * @param whichDB index of the database to which should perform the redo.  This database must be implemented via a UndoStackMessageTreeDatabaseObject,
     *                or the redo request will be ignored.  Defaults to zero.
     * @param optOpTag An optional string to associate with this operation.  It can be anything you like; it will be passed on verbatim to the TreeNodeUpdated()
     *                 callbacks that subscribed ITreeGatewaySubscriber objects receive as a result of this operation.  Defaults to an empty string.
     */
   virtual status_t RequestRedo(uint32 whichDB = 0, const String & optOpTag = GetEmptyString());

   /** Returns true iff our gateway is currently connected to the upstream database. */
   virtual bool IsTreeGatewayConnected() const;

   /** Returns a Message containing various information about the server we are currently connected to.
     * (in particular this Message will contain the ZG_PARAMETER_NAME_* fields listed in TreeConstants.h)
     * @note this method will return a NULL ConstMessageRef if called when we are not currently connected to a server!
     */
   virtual ConstMessageRef GetGestaltMessage() const;

private:
   friend class ITreeGateway;
   friend class GatewaySubscriberUndoBatchGuard;
};

/** RIAA stack-guard object to begin and end an ITreeGatewaySubscriber's Undo Batch at the appropriate times */
class GatewaySubscriberUndoBatchGuard : public NotCopyable
{
public:
   /**
    * Constructor
    * @param sub pointer to the subscriber object to call BeginUndo() on
    * @param label a human-readable label used to describe the undo-action to the user.  Defaults to an empty string.
    * @param whichDB index of the database that the action is going to operate on.  Defaults to 0.
    */
   GatewaySubscriberUndoBatchGuard(ITreeGatewaySubscriber* sub, const String& label = GetEmptyString(), uint32 whichDB = 0) : _sub(sub), _label(label), _whichDB(whichDB)
   {
      _sub->BeginUndoSequence(label, whichDB);
   }
    
   /**
    * Destructor
    * Calls EndUndoSequence on the subscriber object
    */
   ~GatewaySubscriberUndoBatchGuard() {_sub->EndUndoSequence(_label, _whichDB);}
    
private:
   ITreeGatewaySubscriber * _sub;
   const String _label;
   const uint32 _whichDB;
};

};  // end namespace zg

#endif

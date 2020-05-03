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
   NUM_TREE_GATEWAY_FLAGS          /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(TreeGatewayFlags, NUM_TREE_GATEWAY_FLAGS);

class ITreeGateway;

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
     * Default implementation is a no-op.
     */
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg) {(void) nodePath; (void) optPayloadMsg;}

   /** Called by the upstream gateway to notify this subscriber when the node-index of a subscribed-to database node has been cleared.
     * @param nodePath the session-relative path of the database node in question.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeIndexCleared(const String & nodePath) {(void) nodePath;}

   /** Called by the upstream gateway to notify this subscriber when a new entry has been inserted into the node-index of a subscribed-to database node.
     * @param nodePath the session-relative path of the database node in question.
     * @param insertedAtIndex position at which the entry has been inserted (0==first in the index, 1==second in the index, etc)
     * @param nodeName the name of the child node that was inserted at the specified position.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeIndexEntryInserted(const String & nodePath, uint32 insertedAtIndex, const String & nodeName) {(void) nodePath; (void) insertedAtIndex; (void) nodeName;}

   /** Called by the upstream gateway to notify this subscriber when an entry has been removed from the node-index of a subscribed-to database node.
     * @param nodePath the session-relative path of the database node in question.
     * @param insertedAtIndex position from which the entry has been removed (0==first in the index, 1==second in the index, etc)
     * @param nodeName the name of the child node that was removed from the specified position.
     * Default implementation is a no-op.
     */
   virtual void TreeNodeIndexEntryRemoved(const String & nodePath, uint32 removedAtIndex, const String & nodeName) {(void) nodePath; (void) removedAtIndex; (void) nodeName;}

   /** Called when a "pong" comes back from the server (in response to a previous call to PingTreeServer()).
     * @param tag the tag-string that you had previously passed to PingTreeServer().
     * Default implementation is a no-op.
     */
   virtual void TreeServerPonged(const String & tag) {(void) tag;}

   /** Called when a "pong" comes back from the senior peer (in response to a previous call to PingSeniorPeer()).
     * @param whichDB index of the ZG database whose database-update mechanisms the senior-peer-ping had travelled through.
     * @param tag the tag-string that was previously passed to PingSeniorPeer().
     * Default implementation is a no-op.
     */
   virtual void TreeSeniorPeerPonged(uint32 whichDB, const String & tag) {(void) whichDB; (void) tag;}

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
     * @param subscriptionPath the session-relative path of the node(s) you wish to query (e.g. "foo/bar/ba*")
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
     * @param maxDepth The maximum depth of the subtrees to return.  Pass MUSCLE_NO_LIMIT if you want the full subtree, regardless of how deep it goes.
     * @param flags If specified, these flags can influence the behavior of the subscribe operation.  Currently this argument is ignored.
     * @note notifications in response to this query will come in the form of some future calls to TreeNodeUpdated().
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Request that the given specified payload be uploaded to the database.
     * @param nodePath the session-relative path of the node you wish to upload (e.g. "foo/bar/baz").  May be wildcarded only if (optPayload) is NULL.
     *                 If this path ends with a slash, then the server will choose a unique name for the uploaded node.
     * @param optPayload the payload Message to upload, or a NULL reference if you wish to delete one or more nodes instead.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently TREE_GATEWAY_FLAG_INDEXED will cause the
     *              uploaded node to be added to its parent's ordered-children index, and TREE_GATEWAY_FLAG_NOREPLY will prevent any notifications about
     *              this upload from being delivered to any clients (including the caller)
     * @param optBefore Specifies where in the parent's node-index this node should be inserted.  Only used if TREE_GATEWAY_FLAG_INDEXED was specified in the (flags)
     *                  argument.  If non-NULL, the node will be inserted directly before the child node with the specified name; or if NULL, the node will be
     *                  inserted at the front of the index.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t UploadTreeNodeValue(const String & nodePath, const MessageRef & optPayload, TreeGatewayFlags flags = TreeGatewayFlags(), const char * optBefore = NULL);

   /** Request that a subtree of nodes be uploaded to the specified location in the database.
     * @param nodePath the session-relative path indicating where the root of the subtree should be created.
     * @param valuesMsg a Message containing the contents of the subtree (as previously passed back to you in SubtreesRequestResultReturned())
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t UploadTreeNodeSubtree(const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Request that one of more nodes be deleted from the database.
     * @param nodePath Session-relative path to the node (or nodes) to delete.  May be wildcarded.
     * @param optFilterRef if non-NULL, reference to a QueryFilter object that the server will use to limit which nodes get deleted.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @note that any children/grandchildren/etc of deleted nodes will also be deleted.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestDeleteTreeNodes(const String & nodePath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());

   /** Request that an entry within the index of parent-node of one or more specified nodes be moved to a different location in its index.
     * @param nodePath Session-relative path to the node (or nodes) to move within their parents' node-indices.  May be wildcarded (although I'm not sure how useful that is).
     * @param optBefore Specifies where in the parent's node-index the node should be moved to.
     * @param optFilterRef if non-NULL, reference to a QueryFilter object that the server will use to limit which nodes get moved.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t RequestMoveTreeIndexEntry(const String & nodePath, const char * optBefore, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());

   /** Sends a "Ping" message to the local server.
     * @param tag an arbitrary string to send with the ping-message.  Will be sent back verbatim in the corresponding TreeServerPonged() callback.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t PingTreeServer(const String & tag, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Sends a "Ping" message to the senior peer.  This Ping message will go through the entire ZG-database-update meat-grinder before 
     * coming back to you in the form of a call to TreeSeniorPeerPonged(), which is useful if you are trying to synchronize your actions
     * with respect to how a database is being updated system-wide.
     * @param tag an arbitrary string to send with the ping-message.  Will be sent back verbatim in the corresponding SeniorPeerPonged() callback.
     * @param flags If specified, these flags can influence the behavior of the upload operation.  Currently this argument is ignored.
     * @returns B_NO_ERROR on success, or some other error value on failure.
     */
   virtual status_t PingTreeSeniorPeer(uint32 whichDB, const String & tag, TreeGatewayFlags flags = TreeGatewayFlags());

   /** Returns true iff our gateway is currently connected to the upstream database. */
   virtual bool IsTreeGatewayConnected() const;

private:
   friend class ITreeGateway;
};

};  // end namespace zg

#endif

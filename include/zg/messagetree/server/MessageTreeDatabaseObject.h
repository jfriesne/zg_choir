#ifndef MessageTreeDatabaseObject_h
#define MessageTreeDatabaseObject_h

#include "zg/IDatabaseObject.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"  // for TreeGatewayFlags
#include "reflector/StorageReflectSession.h"  // for SetDataNodeFlags
#include "util/NestCount.h"

namespace zg
{

class MessageTreeDatabasePeerSession;

/** This is a concrete implementation of IDatabaseObject that uses a subtree of the MUSCLE 
  * Message-tree database as the data structure it synchronizes across peers.
  */
class MessageTreeDatabaseObject : public IDatabaseObject
{
public:
   /** Constructor
     * @param session pointer to the MessageTreeDatabasePeerSession object that created us
     * @param dbIndex our index within the databases-list.
     * @param rootNodePath a sub-path indicating where the root of our managed Message sub-tree
     *                     should be located (relative to the MessageTreeDatabasePeerSession's session-node)
     *                     May be empty if you want the session's session-node itself of be the
     *                     root of the managed sub-tree.
     */
   MessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath);

   /** Destructor */
   virtual ~MessageTreeDatabaseObject() {/* empty */}

   // IDatabaseObject implementation
   virtual void SetToDefaultState();
   virtual status_t SetFromArchive(const ConstMessageRef & archive);
   virtual status_t SaveToArchive(const MessageRef & archive) const;
   virtual uint32 GetCurrentChecksum() const {return _checksum;}
   virtual uint32 CalculateChecksum() const;
   virtual String ToString() const;

   /** Returns a pointer to the MessageTreeDatabasePeerSession object that created us, or NULL
     * if this object was not created by a MessageTreeDatabasePeerSession.
     */
   MessageTreeDatabasePeerSession * GetMessageTreeDatabasePeerSession() const;

   /** Checks if the given path belongs to this database.
     * @param path a session-relative node-path (e.g. "dbs/db_0/foo/bar"), or an absolute node-path (e.g. "/zg/0/dbs/db_0/foo/bar").
     * @param optRetRelativePath if non-NULL, and this method returns true, then the String this points to will
     *                           be written to with the path to the node that is relative to our root-node (e.g. "foo/bar").
     * @returns The distance between path and our root-node, in "hops", on success (e.g. 0 means the path matches our database's
     *          root-node exactly; 1 means it matches at the level of our database's children, and so on).
     *          Returns -1 if the path doesn't match anything in our database.
     */
   int32 GetDatabaseSubpath(const String & path, String * optRetRelativePath = NULL) const;

   /** Returns the session-relative path of the root of this database's subtree (without any trailing slash) */
   const String & GetRootPathWithoutSlash() const {return _rootNodePathWithoutSlash;}

   /** Returns the session-relative path of the root of this database's subtree (with a trailing slash) */
   const String & GetRootPathWithSlash() const {return _rootNodePathWithSlash;}

   /** Sends a request to the senior peer that the specified node-value be uploaded to the message-tree database.
     * @param path session-relative path of the database node to upload (may be wildcarded, but only if (optPayload) is a NULL reference)
     * @param optPayload reference to the Message payload you want added/updated at the given path, or a NULL reference if you want 
     *                   node(s) matching the given path to be deleted.
     * @param flags optional TREE_GATEWAY_* flags to modify the behavior of the upload.
     * @param optBefore if non-NULL, the name of the sibling node that this node should be placed before, or NULL if you want the
     *                  uploaded node to be placed at the end of the index.  Only used if TREE_GATEWAY_FLAG_INDEXED was specified.
     * @returns B_NO_ERROR on success, or another error code on failure.
     */
   virtual status_t UploadNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore);

   /** Sends a request to the senior peer that the specified node sub-tree be uploaded.
     * @param path session-relative path indicating where in the message-tree to place the root of the uploaded sub-tree.
     * @param valuesMsg should contain the subtree to upload.
     * @param flags optional TREE_GATEWAY_* flags to modify the behavior of the upload.
     * @returns B_NO_ERROR on success, or another error code on failure.
     */
   virtual status_t UploadNodeSubtree(const String & path, const MessageRef & valuesMsg, TreeGatewayFlags flags);

   /** Sends a request to remove matching nodes from the database.
     * @param path session-relative path indicating which node(s) to delete.  May be wildcarded.
     * @param optFilter if non-NULL, only nodes whose Message-payloaded are matched by this query-filter will be deletes.
     * @param flags optional TREE_GATEWAY_* flags to modify the behavior of the operation.
     * @returns B_NO_ERROR on success, or another error code on failure.
     */
   virtual status_t RequestDeleteNodes(const String & path, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags);

   /** Sends a request to modify the ordering of the indices of matching nodes in the database.
     * @param path session-relative path indicating which node(s) to modify the indices of.  May be wildcarded.
     * @param optBefore if non-NULL, the name of the sibling node that this node should be placed before, or NULL if you want the
     *                  uploaded node to be placed at the end of the index.  Only used if TREE_GATEWAY_FLAG_INDEXED was specified.
     * @param optFilter if non-NULL, only nodes whose Message-payloaded are matched by this query-filter will have their indices modified.
     * @param flags optional TREE_GATEWAY_* flags to modify the behavior of the operation.
     * @returns B_NO_ERROR on success, or another error code on failure.
     */
   virtual status_t RequestMoveIndexEntry(const String & path, const String * optBefore, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags);

   /** This callback method is called when a node in this database-object's subtree is created, updated, or destroyed.
     * @param relativePath the path to this node (relative to this database-object's root-node)
     * @param node a reference to the node's current state -- see node.GetData() for the node's current (post-change) payload. 
     * @param oldDataRef a reference to the node's payload as it was before this change (or a NULL reference if this node is being created now)
     * @param isBeingRemoved true iff this node is being deleted by this change
     * @note be sure to call up to the parent implementation of this method if you override it!
     */
   virtual void MessageTreeNodeUpdated(const String & relativePath, DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved);

   /** This callback method is called when the index of node in this database-object's subtree is being modified.
     * @param relativePath the path to this node (relative to this database-object's root-node)
     * @param node a reference to the node's current state
     * @param op An INDEX_OP_* value indicating what type of index-update is being applied to the index.
     * @param index the location within the index at which the new entry is to be inserted (if op is INDEX_OP_ENTRY_INSERTED)
     *              or removed (if op is INDEX_OP_ENTRYREMOVED).  Not used if op is INDEX_OP_ENTRYCLEARED.
     * @param key Name of the node that is being inserted (if op is INDEX_OP_ENTRYINSERTED) or removed (if op is INDEX_OP_ENTRYREMOVED)
     *            Not used if op is INDEX_OP_ENTRYCLEARED.
     * @note be sure to call up to the parent implementation of this method if you override it!
     */
   virtual void MessageTreeNodeIndexChanged(const String & relativePath, DataNode & node, char op, uint32 index, const String & key);

   /** Called after an ITreeGatewaySubscriber somewhere has called SendMessageToTreeSeniorPeer().
     * @param fromPeerID the ID of the ZGPeer that the subscriber is directly connected to
     * @param payload the Message that the subscriber sent to us
     * @param tag a tag-string that can be used to route replies back to the originating subscriber, if desired.
     * @note Default implementation just prints an error to the log saying that the Message wasn't handled.  
     */
   virtual void MessageReceivedFromTreeGatewaySubscriber(const ZGPeerID & fromPeerID, const MessageRef & payload, const String & tag);

   /** Call this to send a Message back to an ITreeGatewaySubscriber (e.g. in response to a MessageReceivedFromTreeGatewaySubscriber() callback)
     * @param toPeerID the ID of the ZGPeer that the subscriber is directly connected to
     * @param tag the tag-String to use to direct the Message to the correct subscriber (as was previously passed in to MessageReceivedFromTreeGatewaySubscriber())
     * @param payload the Message to send to the subscriber
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t SendMessageToTreeGatewaySubscriber(const ZGPeerID & toPeerID, const String & tag, const MessageRef & payload); 
     
protected:
   // IDatabaseObject API
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);

   /** Called by SeniorUpdate() when it wants to add a set/remove-node action to the Junior-Message it is assembling for junior peers to act on when they update their databases.
     * Default implementation just adds the appropriate update-Message to (assemblingMessage), but subclasses can
     * override this to do more (e.g. to also record undo-stack information, in the UndoStackMessageTreeDatabaseObject subclass).
     * @param relativePath path to the node in question, relative to our subtree's root.
     * @param oldPayload the payload that our node had before we made this change (NULL if the node is being created)
     * @param newPayload the payload that our node has after we make this change (NULL if the node is being destroyed)
     * @param assemblingMessage the Message we are gathering records into.
     * @param prepend True iff the filed Message should be prepended to the beginning of (assemblingMessage), or false if it should be appended to the end.
     * @returns a valid MessageRef on success, or a NULL MessageRef on failure.
     */
   virtual status_t SeniorRecordNodeUpdateMessage(const String & relativePath, const MessageRef & oldPayload, const MessageRef & newPayload, MessageRef & assemblingMessage, bool prepend);

   /** Called by SeniorUpdate() when it wants to add an update-node-index action to the Junior-Message it is assembling for junior peers to act on when they update their databases.
     * Default implementation just adds the appropriate update-Message to (assemblingMessage), but subclasses can
     * override this to do more (e.g. to also record undo-stack information, in the UndoStackMessageTreeDatabaseObject subclass).
     * @param relativePath path to the node in question, relative to our subtree's root.
     * @param op the index-update opcode of the change
     * @param index the position within the index of the change
     * @param key the name of the child node in the index
     * @param assemblingMessage the Message we are gathering records into.
     * @param prepend True iff the filed Message should be prepended to the beginning of (assemblingMessage), or false if it should be appended to the end.
     * @returns a valid MessageRef on success, or a NULL MessageRef on failure.
     */
   virtual status_t SeniorRecordNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, MessageRef & assemblingMessage, bool prepend);

   /** Called by SeniorUpdate() when it needs to handle an individual sub-Message in the senior-update context
     * @param msg the sub-Message to handle
     * @returns B_NO_ERROR on success, or another error-code on failure.
     * @note the default implementation handles the standard Message-Tree functionality, but subclasses can override this to provide more handling if they want to.
     */
   virtual status_t SeniorMessageTreeUpdate(const ConstMessageRef & msg);

   /** Called by SeniorUpdate() when it needs to handle an individual sub-Message in the junior-update context
     * @param msg the sub-Message to handle
     * @returns B_NO_ERROR on success, or another error-code on failure.
     * @note the default implementation handles the standard Message-Tree functionality, but subclasses can override this to provide more handling if they want to.
     */
   virtual status_t JuniorMessageTreeUpdate(const ConstMessageRef & msg);

   /** Used to decide whether or not to handle a given MTD_COMMAND_* update Message.
     * Used as a hook by the UndoStackMessageTreeDatabaseObject subclass to filter out undesirable meta-data updates
     * when executing an "undo" or a "redo" operation.  Default implementation just always returns true.
     * @param path the node-path specified by the update-message
     * @param flags the TreeGatewayFlags specified by the update-message
     */
   virtual bool IsOkayToHandleUpdateMessage(const String & path, TreeGatewayFlags flags) const {(void) path; (void) flags; return true;}

   /** When called from within a SeniorUpdate() or JuniorUpdate() context, returns true iff the update we're currently
     * handling was tagged with the TREE_GATEWAY_FLAG_INTERIM (and can therefore be skipped when performing an undo or redo)
     */
   bool IsHandlingInterimUpdate() const {return _interimUpdateNestCount.IsInBatch();}

   /**
    * Returns a pointer to the first DataNode object that mactches the given node-path.
    * @param nodePath The node's path, relative to this database object's root-path.  Wildcarding is okay.
    * @return A pointer to the specified DataNode, or NULL if no node matching that path was found.
    */
   DataNode * GetDataNode(const String & nodePath) const;

   /** Pass-through to StorageReflectSession::SetDataNode() on our MessageTreeDatabasePeerSession object
     * @param nodePath The node's path, relative to this database object's root-path.
     * @param dataMsgRef The value to set the node to
     * @param flags list of SETDATANODE_FLAG_* values to affect our behavior.  Defaults to no-bits-set.
     * @param optInsertBefore If (addToIndex) is true, this may be the name of the node to insert this new node before in the index.
     *                        If NULL, the new node will be appended to the end of the index.  If (addToIndex) is false, this argument is ignored.
     * @return B_NO_ERROR on success, or an error code on failure.
     */
   status_t SetDataNode(const String & nodePath, const MessageRef & dataMsgRef, SetDataNodeFlags flags=SetDataNodeFlags(), const String *optInsertBefore=NULL); 

   /** Convenience method:  Adds nodes that match the specified path to the passed-in Queue.
    *  @param nodePath the node path to match against.  May be absolute (e.g. "/0/1234/frc*") or relative (e.g. "blah").
    *                  If it's a relative path, only nodes in the current session's subtree will be searched.
    *  @param filter If non-NULL, only nodes whose data Messages match this filter will be added to the (retMatchingNodes) table.
    *  @param retMatchingNodes A Queue that will on return contain the list of matching nodes.
    *  @param maxResults Maximum number of matching nodes to return.  Defaults to MUSCLE_NO_LIMIT.
    *  @return B_NO_ERROR on success, or an error code on failure.  Note that failing to find any matching nodes is NOT considered an error.
    */
   status_t FindMatchingNodes(const String & nodePath, const ConstQueryFilterRef & filter, Queue<DataNodeRef> & retMatchingNodes, uint32 maxResults = MUSCLE_NO_LIMIT) const;

   /** Convenience method:  Same as FindMatchingNodes(), but finds only the first matching node.
     *  @param nodePath the node path to match against.  May be absolute (e.g. "/0/1234/frc*") or relative (e.g. "blah").
     *                  If it's a relative path, only nodes in the current session's subtree will be searched.
     *  @param filter If non-NULL, only nodes whose data Messages match this filter will be added to the (retMatchingNodes) table.
     *  @returns a reference to the first matching node on success, or a NULL reference on failure.
     */
   DataNodeRef FindMatchingNode(const String & nodePath, const ConstQueryFilterRef & filter) const;

   /** Convenience method (used by some customized daemons) -- Given a source node and a destination path,
    * Make (path) a deep, recursive clone of (node).
    * @param sourceNode Reference to a DataNode to clone.
    * @param destPath Path of where the newly created node subtree will appear.  Should be relative to our home node.
    * @param flags optional bit-chord of SETDATANODE_FLAG_* flags to modify our behavior.  Defaults to no-flags-set.
    * @param optInsertBefore If (addToTargetIndex) is true, this argument will be passed on to InsertOrderedChild().
    *                        Otherwise, this argument is ignored.
    * @param optPruner If non-NULL, this object can be used as a callback to prune the traversal or filter
    *                  the MessageRefs cloned.
    * @return B_NO_ERROR on success, or an error code on failure (may leave a partially cloned subtree on failure)
    */
   status_t CloneDataNodeSubtree(const DataNode & sourceNode, const String & destPath, SetDataNodeFlags flags = SetDataNodeFlags(), const String * optInsertBefore = NULL, const ITraversalPruner * optPruner = NULL);

   /** Pass-through to StorageReflectSession::RemoveDataNodes() on our MessageTreeDatabasePeerSession object
     * @param nodePath The node's path, relative to this database object's root-path.  Wildcarding is okay.
     * @param filterRef optional ConstQueryFilter to restrict which nodes that match (nodePath) actually get removed
     * @param quiet If set to true, subscribers won't be updated regarding this change to the database.
     * @return B_NO_ERROR on success, or an error code on failure.
     */
   status_t RemoveDataNodes(const String & nodePath, const ConstQueryFilterRef & filterRef = ConstQueryFilterRef(), bool quiet = false);

   /** Pass-through to StorageReflectSession::MoveIndexEntries() on our MessageTreeDatabasePeerSession object
     * @param nodePath The node's path, relative to this database object's root-path.  Wildcarding is okay.
     * @param optBefore if non-NULL, the moved nodes in the index will be moved to just before the node with this name.  If NULL, they'll be moved to the end of the index.
     * @param filterRef If non-NULL, we'll use the given QueryFilter object to filter out our result set.
     *                  Only nodes whose Messages match the QueryFilter will have their parent-nodes' index modified.  Default is a NULL reference.
     * @return B_NO_ERROR on success, or an error code on failure.
     */
   status_t MoveIndexEntries(const String & nodePath, const String * optBefore, const ConstQueryFilterRef & filterRef);

private:
   class SafeQueryFilter : public QueryFilter
   {
   public:
      SafeQueryFilter(const MessageTreeDatabaseObject * dbObj) : _dbObj(dbObj) {/* empty */}
   
      virtual bool Matches(ConstMessageRef & /*msg*/, const DataNode * optNode) const {return optNode ? _dbObj->IsNodeInThisDatabase(*optNode) : false;}
      virtual uint32 TypeCode() const {return 0;}  // okay because we never save/restore this type anyway
      virtual status_t SaveToArchive(Message &) const  {MCRASH("SafeQueryFilter shouldn't be saved to an archive"); return B_UNIMPLEMENTED;}
      virtual status_t SetFromArchive(const Message &) {MCRASH("SafeQueryFilter shouldn't be set from an archive"); return B_UNIMPLEMENTED;}

   private:
      const MessageTreeDatabaseObject * _dbObj;
   };

   bool IsInSetupOrTeardown() const;
   bool IsNodeInThisDatabase(const DataNode & node) const;
   String DatabaseSubpathToSessionRelativePath(const String & subPath) const {return subPath.HasChars() ? _rootNodePathWithoutSlash.AppendWord(subPath, "/") : _rootNodePathWithoutSlash;}
   void DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const;

   MessageRef CreateNodeUpdateMessage(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore) const;
   MessageRef CreateNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key);
   MessageRef CreateSubtreeUpdateMessage(const String & path, const MessageRef & payload, TreeGatewayFlags flags) const;

   status_t HandleNodeUpdateMessage(const Message & msg);
   status_t HandleNodeUpdateMessageAux(const Message & msg, TreeGatewayFlags flags);
   status_t HandleNodeIndexUpdateMessage(const Message & msg);
   status_t HandleSubtreeUpdateMessage(const Message & msg);

   status_t UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabel, uint32 whichDB);

   MessageRef _assembledJuniorMessage;
   NestCount _interimUpdateNestCount;

   const String _rootNodePathWithoutSlash;
   const String _rootNodePathWithSlash;
   const uint32 _rootNodeDepth;
   uint32 _checksum;  // running checksum
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

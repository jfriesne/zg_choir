#ifndef MessageTreeDatabaseObject_h
#define MessageTreeDatabaseObject_h

#include "zg/IDatabaseObject.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"  // for TreeGatewayFlags
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

   virtual void MessageTreeNodeUpdated(const String & relativePath, DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved);
   virtual void MessageTreeNodeIndexChanged(const String & relativePath, DataNode & node, char op, uint32 index, const String & key);

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

   /** When called from within a SeniorUpdate() or JuniorUpdate() context, returns true iff the update we're currently
     * handling was tagged with the TREE_GATEWAY_FLAG_INTERIM (and can therefore be skipped when performing an undo or redo)
     */
   bool IsHandlingInterimUpdate() const {return _interimUpdateNestCount.IsInBatch();}

   /** Returns a reference to a NestCount that can be adjusted to indicate when we're operating in the context of an undo or redo operation */
   NestCount & GetInUndoRedoContextNestCount() {return _inUndoRedoContextNestCount;}

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
   status_t SafeRemoveDataNodes(const String & nodePath, const ConstQueryFilterRef & filterRef = ConstQueryFilterRef(), bool quiet = false);
   status_t SafeMoveIndexEntries(const String & nodePath, const String * optBefore, const ConstQueryFilterRef & filterRef);

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

   NestCount _inUndoRedoContextNestCount;

   const String _rootNodePathWithoutSlash;
   const String _rootNodePathWithSlash;
   const uint32 _rootNodeDepth;
   uint32 _checksum;  // running checksum
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

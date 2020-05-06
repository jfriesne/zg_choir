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
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);
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
   const String & GetRootPath() const {return _rootNodePathWithoutSlash;}

   status_t UploadNodeValue(const String & path, const MessageRef & optPaylod, TreeGatewayFlags flags, const char * optBefore);
   status_t UploadNodeSubtree(const String & path, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   status_t RequestDeleteNodes(const String & path, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags);
   status_t RequestMoveIndexEntry(const String & path, const char * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);

   virtual void MessageTreeNodeUpdated(const String & relativePath, DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved);
   virtual void MessageTreeNodeIndexChanged(const String & relativePath, DataNode & node, char op, uint32 index, const String & key);

private:
   class SafeQueryFilter : public QueryFilter
   {
   public:
      SafeQueryFilter(const MessageTreeDatabaseObject * dbObj) : _dbObj(dbObj) {/* empty */}
   
      virtual bool Matches(ConstMessageRef & /*msg*/, const DataNode * optNode) const {return optNode ? _dbObj->IsNodeInThisDatabase(*optNode) : false;}
      virtual uint32 TypeCode() const {return 0;}
   
   private:
      const MessageTreeDatabaseObject * _dbObj;
   };

   bool IsInSetupOrTeardown() const;
   bool IsNodeInThisDatabase(const DataNode & node) const;
   String DatabaseSubpathToSessionRelativePath(const String & subPath) const {return subPath.HasChars() ? _rootNodePathWithoutSlash.AppendWord(subPath, "/") : _rootNodePathWithoutSlash;}
   void DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const;
   status_t SafeRemoveDataNodes(const String & nodePath, const ConstQueryFilterRef & filterRef = ConstQueryFilterRef(), bool quiet = false);
   status_t SafeMoveIndexEntries(const String & nodePath, const String * optBefore, const ConstQueryFilterRef & filterRef);

   status_t SeniorUpdateAux(const ConstMessageRef & msg);
   status_t JuniorUpdateAux(const ConstMessageRef & msg);

   MessageRef CreateNodeUpdateMessage(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore) const;
   MessageRef CreateNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key);
   MessageRef CreateSubtreeUpdateMessage(const String & path, const MessageRef & payload, TreeGatewayFlags flags) const;

   status_t HandleNodeUpdateMessage(const Message & msg);
   status_t HandleNodeIndexUpdateMessage(const Message & msg);
   status_t HandleSubtreeUpdateMessage(const Message & msg);

   MessageRef _assembledJuniorMessage;

   const String _rootNodePathWithoutSlash;
   const String _rootNodePathWithSlash;
   const uint32 _rootNodeDepth;
   uint32 _checksum;  // running checksum
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

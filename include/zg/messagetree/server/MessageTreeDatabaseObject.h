#ifndef MessageTreeDatabaseObject_h
#define MessageTreeDatabaseObject_h

#include "zg/IDatabaseObject.h"
#include "util/NestCount.h"

namespace zg
{

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
   MessageTreeDatabasePeerSession * GetMessageTreeDatabasePeerSession() const {return static_cast<MessageTreeDatabasePeerSession *>(GetDatabasePeerSession());}

   /** Checks if the given path belongs to this database.
     * @param path a session-relative node-path (e.g. "dbs/db_0/foo/bar"), or an absolute node-path (e.g. "/zg/0/dbs/db_0/foo/bar").
     * @param optRetRelativePath if non-NULL, and this method returns true, then the String this points to will
     *                           be written to with the path to the node that is relative to our root-node (e.g. "foo/bar").
     * @returns B_NO_ERROR iff the given relative-node-path is within our sub-tree
     */
   status_t GetDatabaseSubpath(const String & path, String * optRetRelativePath = NULL) const;

   /** Returns the number of "hops upstream" it takes to get from (path) to this MessageTreeDatabaseObject's root-node.
     * @param path a session-relative node-path (e.g. "dbs/db_0/foo/bar"), or an absolute node-path (e.g. "/zg/0/dbs/db_0/foo/bar").
     * @returns 0 if (path) is our database's root-node, 1 if it is a child of the root-node, 2 if it's a grandchild, and so on.
     *          Returns -1 if the given path is outside of our root-node's subtree.
     */
   int32 GetDistanceFromDatabaseRootToNode(const String & path) const;

   status_t UploadNodeValue(const String & path, const MessageRef & optPaylod, TreeGatewayFlags flags, const char * optBefore);
   status_t RequestDeleteNodes(const String & path, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags);
   status_t RequestMoveIndexEntry(const String & path, const char * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);

   virtual void MessageTreeNodeUpdated(const String & relativePath, DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved);
   virtual void MessageTreeNodeIndexChanged(const String & relativePath, DataNode & node, char op, uint32 index, const String & key);

private:
   String DatabaseSubpathToSessionRelativePath(const String & subPath) const {return subPath.HasChars() ? subPath.Prepend(_rootNodePathWithSlash) : _rootNodePathWithoutSlash;}
   void DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const;
   status_t SeniorUpdateAux(const ConstMessageRef & msg);
   status_t JuniorUpdateAux(const ConstMessageRef & msg);

   MessageRef CreateNodeUpdateMessage(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore) const;
   status_t HandleUpdateNodeMessage(const Message & msg);

   MessageRef CreateUpdateNodeIndexMessage(const String & relativePath, char op, uint32 index, const String & key);
   status_t HandleUpdateNodeIndexMessage(const Message & msg);

   NestCount _inSeniorUpdateNestCount;
   NestCount _inJuniorUpdateNestCount;

   MessageRef _assembledJuniorMessage;

   const String _rootNodePathWithoutSlash;
   const String _rootNodePathWithSlash;
   uint32 _checksum;  // running checksum
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

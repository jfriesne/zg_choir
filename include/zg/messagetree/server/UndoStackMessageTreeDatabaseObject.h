#ifndef UndoStackMessageTreeDatabaseObject_h
#define UndoStackMessageTreeDatabaseObject_h

#include "zg/messagetree/server/MessageTreeDatabaseObject.h"

namespace zg
{

class ServerSideMessageTreeSession;

/** This is a special subclass of MessageTreeDatabaseObject that implements
  * a per-client undo/redo stack as part of its operation.  You can use this 
  * if you want to be able to support undo and redo operations as part of
  * your user interface.
  */
class UndoStackMessageTreeDatabaseObject : public MessageTreeDatabaseObject 
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
   UndoStackMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath);

   /** Destructor */
   virtual ~UndoStackMessageTreeDatabaseObject() {/* empty */}

protected:
   virtual MessageRef SeniorCreateNodeUpdateMessage(const String & relativePath, const DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved) const;
   virtual MessageRef SeniorCreateNodeIndexUpdateMessage(const String & relativePath, const DataNode & node, char op, uint32 index, const String & key);
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

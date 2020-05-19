#ifndef UndoStackMessageTreeDatabaseObject_h
#define UndoStackMessageTreeDatabaseObject_h

#include "zg/messagetree/server/MessageTreeDatabaseObject.h"

namespace zg
{

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
   // IDatabaseObject API
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);

   virtual status_t SeniorRecordNodeUpdateMessage(const String & relativePath, const MessageRef & oldPayload, const MessageRef & newPayload, MessageRef & assemblingMessage, bool prepend);
   virtual status_t SeniorRecordNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, MessageRef & assemblingMessage, bool prepend);

private:
   MessageRef _assembledJuniorUndoMessage;
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

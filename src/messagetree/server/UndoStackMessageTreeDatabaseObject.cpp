#include "zg/messagetree/server/UndoStackMessageTreeDatabaseObject.h"

namespace zg 
{

UndoStackMessageTreeDatabaseObject :: UndoStackMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : MessageTreeDatabaseObject(session, dbIndex, rootNodePath)
{
   // empty
}

MessageRef UndoStackMessageTreeDatabaseObject :: SeniorCreateNodeUpdateMessage(const String & relativePath, const DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved) const
{  
   MessageRef ret = MessageTreeDatabaseObject::SeniorCreateNodeUpdateMessage(relativePath, node, oldDataRef, isBeingRemoved);
   if (ret() == NULL) return MessageRef();

//printf("SeniorCreateUndo!\n");
   return ret;
}


MessageRef UndoStackMessageTreeDatabaseObject :: SeniorCreateNodeIndexUpdateMessage(const String & relativePath, const DataNode & node, char op, uint32 index, const String & key)
{
   MessageRef ret = MessageTreeDatabaseObject::SeniorCreateNodeIndexUpdateMessage(relativePath, node, op, index, key);
   if (ret() == NULL) return MessageRef();

//printf("SeniorCreateUndoIndex!\n");
   return ret;
}


}; // end namespace zg

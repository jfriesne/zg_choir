#include "zg/messagetree/server/UndoStackMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "reflector/StorageReflectConstants.h"  // for INDEX_OP_*

namespace zg 
{

// UndoStackMessageTreeDatabaseObject will store its transactions as do/undo Message-pairs
enum {
   UNDOSTACK_COMMAND_MESSAGE_PAIR = 1970168943 // 'undo' 
};
static const String UNDOSTACK_NAME_DOMESSAGE   = "_do";
static const String UNDOSTACK_NAME_UNDOMESSAGE = "_un";
static const String UNDOSTACK_NAME_UNDOKEY     = "_ky";

UndoStackMessageTreeDatabaseObject :: UndoStackMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : MessageTreeDatabaseObject(session, dbIndex, rootNodePath)
{
   // empty
}

ConstMessageRef UndoStackMessageTreeDatabaseObject :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   MessageRef doMsg = CastAwayConstFromRef(MessageTreeDatabaseObject::SeniorUpdate(seniorDoMsg));
   if (doMsg() == NULL) return ConstMessageRef();

   MessageRef undoMsg = _assembledJuniorUndoMessage;  // _assembledJuniorUndoMessage will have been populated via callbacks to our methods that occurred insie MessageTreeDatabaseObject::SeniorUpdate()
   _assembledJuniorUndoMessage.Reset();

   MessageRef pairMsg = GetMessageFromPool(UNDOSTACK_COMMAND_MESSAGE_PAIR);
   if ((pairMsg())&&(pairMsg()->CAddMessage(UNDOSTACK_NAME_DOMESSAGE, doMsg).IsOK())&&(pairMsg()->CAddMessage(UNDOSTACK_NAME_UNDOMESSAGE, undoMsg).IsOK())&&(pairMsg()->CAddString(UNDOSTACK_NAME_UNDOKEY, seniorDoMsg()->GetString(UNDOSTACK_NAME_UNDOKEY)).IsOK())) return AddConstToRef(pairMsg);
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject::SeniorUpdate():  Unable to assemble Message-pair!\n");
      return ConstMessageRef();
   }
}

status_t UndoStackMessageTreeDatabaseObject :: JuniorUpdate(const ConstMessageRef & pairMsg)
{
   MessageRef doMsg;
   status_t ret;
   return pairMsg()->FindMessage(UNDOSTACK_NAME_DOMESSAGE, doMsg).IsOK(ret) ? MessageTreeDatabaseObject::JuniorUpdate(doMsg) : ret;
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorRecordNodeUpdateMessage(const String & relativePath, const MessageRef & oldPayload, const MessageRef & newPayload, MessageRef & assemblingMessage, bool prepend)
{
   // File the do-action as usual for our Junior Peers to use
   status_t ret;
   if (MessageTreeDatabaseObject::SeniorRecordNodeUpdateMessage(relativePath, oldPayload, newPayload, assemblingMessage, prepend).IsError(ret)) return ret;

   // Also prepend the equal-and-opposite undo-action to the beginning of our _assembledJuniorUndoMessage (in case we ever want to undo this action later)
   return MessageTreeDatabaseObject::SeniorRecordNodeUpdateMessage(relativePath, newPayload, oldPayload, _assembledJuniorUndoMessage, !prepend);
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorRecordNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, MessageRef & assemblingMessage, bool prepend)
{
   // File the do-action as usual for our Junior Peers to use
   status_t ret;
   if (MessageTreeDatabaseObject::SeniorRecordNodeIndexUpdateMessage(relativePath, op, index, key, assemblingMessage, prepend).IsError(ret)) return ret;

   // Also prepend the equal-and-opposite undo-action to the beginning of our _assembledJuniorUndoMessage (in case we ever want to undo this action later)
   return MessageTreeDatabaseObject::SeniorRecordNodeIndexUpdateMessage(relativePath, (op == (char)INDEX_OP_ENTRYINSERTED) ? INDEX_OP_ENTRYREMOVED : INDEX_OP_ENTRYINSERTED, index, key, assemblingMessage, !prepend);
}

status_t UndoStackMessageTreeDatabaseObject :: RequestReplaceDatabaseState(const MessageRef & newDatabaseStateMsg)
{
   status_t ret;
   return newDatabaseStateMsg()->CAddString(UNDOSTACK_NAME_UNDOKEY, GetActiveClientUndoKey()).IsOK(ret) ? MessageTreeDatabaseObject::RequestReplaceDatabaseState(newDatabaseStateMsg) : ret;
}

status_t UndoStackMessageTreeDatabaseObject :: RequestUpdateDatabaseState(const MessageRef & databaseUpdateMsg)
{
   status_t ret;
   return databaseUpdateMsg()->CAddString(UNDOSTACK_NAME_UNDOKEY, GetActiveClientUndoKey()).IsOK(ret) ? MessageTreeDatabaseObject::RequestUpdateDatabaseState(databaseUpdateMsg) : ret;
}

const String & UndoStackMessageTreeDatabaseObject :: GetActiveClientUndoKey() const
{
   const ServerSideMessageTreeSession * ssmts = GetMessageTreeDatabasePeerSession()->GetActiveServerSideMessageTreeSession();
   return ssmts ? ssmts->GetUndoKey() : GetEmptyString();
}

}; // end namespace zg

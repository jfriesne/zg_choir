#include "zg/messagetree/server/UndoStackMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "reflector/StorageReflectConstants.h"  // for INDEX_OP_*

namespace zg 
{

static const String UNDOSTACK_NAME_DOMESSAGE       = "_do";
static const String UNDOSTACK_NAME_UNDOMESSAGE     = "_un";
static const String UNDOSTACK_NAME_UNDOKEY         = "key";
static const String UNDOSTACK_NAME_LABEL           = "lab";
static const String UNDOSTACK_NAME_CURRENTSEQUENCE = "cur";
static const String UNDOSTACK_NAME_AFTERLASTDBID   = "ald";

static const String UNDOSTACK_NODENAME_UNDO = "undo";
static const String UNDOSTACK_NODENAME_REDO = "redo";


UndoStackMessageTreeDatabaseObject :: UndoStackMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : MessageTreeDatabaseObject(session, dbIndex, rootNodePath)
{
   // empty
}

status_t UndoStackMessageTreeDatabaseObject :: UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabel)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if (msg()->CAddString(UNDOSTACK_NAME_LABEL, optSequenceLabel).IsError(ret)) return ret;

   return RequestUpdateDatabaseState(msg);
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

// Used to match only undo-sequence nodes whose start-transaction-IDs reference transactions we no longer have
class ObsoleteSequencesQueryFilter : public QueryFilter
{
public:
   ObsoleteSequencesQueryFilter(const UndoStackMessageTreeDatabaseObject * master, uint64 curDBID) : _master(master), _curDBID(curDBID) {/* empty */}

   virtual uint32 TypeCode() const {return 0;}  // don't care

   virtual bool Matches(ConstMessageRef &, const DataNode * optNode) const
   {
      if (optNode == NULL) return true;

      const uint64 nodeNameVal = Atoull(optNode->GetNodeName()());
      return ((nodeNameVal != _curDBID)&&(_master->UpdateLogContainsUpdate(nodeNameVal) == false));
   }

private:
   const UndoStackMessageTreeDatabaseObject * _master;
   const uint64 _curDBID;
};

status_t UndoStackMessageTreeDatabaseObject :: SeniorMessageTreeUpdate(const ConstMessageRef & msg)
{
   NestCountGuard ncg(_seniorMessageTreeUpdateNestCount);

   const status_t ret = SeniorMessageTreeUpdateAux(msg);
   if (ret.IsOK())
   {
      // After a successful update, we want to also remove any undo-operations that are no longer
      // possible because the database transactions they reference are no longer in the transaction log
      MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();
      const ObsoleteSequencesQueryFilter osqf(this, GetCurrentDatabaseStateID()+1);  // +1 because the db transaction we are finishing up here hasn't been included in the database yet
      (void) mtdps->RemoveDataNodes(GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO + "/*/*", ConstQueryFilterRef(&osqf, false));

      // Also remove any client-data-nodes that no longer have any children (just to be tidy)
      const ChildCountQueryFilter ccqf(ChildCountQueryFilter::OP_EQUAL_TO, 0);
      (void) mtdps->RemoveDataNodes(GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO + "/*", ConstQueryFilterRef(&ccqf, false));
   }
   return ret;
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorMessageTreeUpdateAux(const ConstMessageRef & msg)
{
   switch(msg()->what)
   {
      case UNDOSTACK_COMMAND_BEGINSEQUENCE: case UNDOSTACK_COMMAND_ENDSEQUENCE:
      {
         const String & clientKey = *(msg()->GetStringPointer(UNDOSTACK_NAME_UNDOKEY, &GetEmptyString()));
         const String nodePath = GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO + "/" + (clientKey.HasChars() ? clientKey : "default");

         status_t ret;
         MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();
         DataNode * clientNode = mtdps->GetDataNode(nodePath);
         if (clientNode == NULL)
         {
            if (mtdps->SetDataNode(nodePath, GetMessageFromPool(0)).IsError(ret)) return ret;  // demand-create an undo-node for this client
            clientNode = mtdps->GetDataNode(nodePath);
         }
         if (clientNode == NULL) return B_BAD_OBJECT;  // wtf?

         // Gotta make a copy of the client-node's payload as we're not allowed to simply modify a DataNode's payload in-place
         MessageRef newClientPayload = GetLightweightCopyOfMessageFromPool(*clientNode->GetData()());
         if (newClientPayload() == NULL) RETURN_OUT_OF_MEMORY;

         uint32 & seqNestCount = newClientPayload()->what;   // I'm using the client-node's what-code as a per-client in-sequence-nest-counter
         if (msg()->what == UNDOSTACK_COMMAND_BEGINSEQUENCE)
         {
            if (++seqNestCount == 1)
            {
               // Entering the first level of undo-sequence nesting -- create a new undo-sequence child node for this client
               MessageRef seqPayload = GetMessageFromPool();
               if (seqPayload() == NULL) RETURN_OUT_OF_MEMORY;

               const String * label = msg()->GetStringPointer(UNDOSTACK_NAME_LABEL);
               if ((label)&&(label->HasChars())&&(seqPayload()->AddString(UNDOSTACK_NAME_LABEL, *label).IsError(ret))) return ret;

               const uint64 startDBID = GetCurrentDatabaseStateID()+1;  // +1 because the db transaction we are part of hasn't been included in the database yet
               const String seqPath = nodePath + String("/%1").Arg(startDBID);
               if (mtdps->SetDataNode(seqPath, seqPayload).IsError(ret)) return ret;

               // Also record in the client-node what our current sequence-ID is
               (void) newClientPayload()->RemoveName(UNDOSTACK_NAME_CURRENTSEQUENCE);
               if (newClientPayload()->AddInt64(UNDOSTACK_NAME_CURRENTSEQUENCE, startDBID).IsError(ret)) return ret;
            }
         }
         else if (seqNestCount > 1) seqNestCount--;
         else
         {
            seqNestCount = 0;

            // Exiting the last level of undo-sequence nesting -- gotta finalize the undo-sequence child node
            uint64 startDBID = 0;
            if (newClientPayload()->FindInt64(UNDOSTACK_NAME_CURRENTSEQUENCE, startDBID).IsOK(ret))
            {
               (void) newClientPayload()->RemoveName(UNDOSTACK_NAME_CURRENTSEQUENCE);  // it's not current anymore!

               const String seqPath = nodePath + String("/%1").Arg(startDBID);
               DataNode * seqNode = mtdps->GetDataNode(seqPath);
               if (seqNode)
               {
                  const uint64 afterLastDBID = GetCurrentDatabaseStateID();  // not +1, for some reason?
                  MessageRef newSeqMsg = GetLightweightCopyOfMessageFromPool(*seqNode->GetData()());
                  if (newSeqMsg() == NULL) RETURN_OUT_OF_MEMORY;

                  if (newSeqMsg()->AddInt64(UNDOSTACK_NAME_AFTERLASTDBID, afterLastDBID).IsError(ret)) return ret;

                  const String * label = msg()->GetStringPointer(UNDOSTACK_NAME_LABEL);
                  if ((label)&&(label->HasChars()))
                  {
                     (void) newSeqMsg()->RemoveName(UNDOSTACK_NAME_LABEL);  // to avoid updating the original/non-lightweight-shared Message
                     if (newSeqMsg()->AddString(UNDOSTACK_NAME_LABEL, *label).IsError(ret)) return ret;
                  }

                  seqNode->SetData(newSeqMsg, mtdps, false);
               }
               else LogTime(MUSCLE_LOG_WARNING, "UndoStackMessageTreeDatabaseObject::SeniorMessageTreeUpdate:  Couldn't find sequence-node at [%s] in UNDOSTACK_COMMAND_ENDSEQUENCE!\n", seqPath());
            }
            else LogTime(MUSCLE_LOG_WARNING, "UndoStackMessageTreeDatabaseObject::SeniorMessageTreeUpdate:  No current-sequence-ID found for client [%s] in UNDOSTACK_COMMAND_ENDSEQUENCE!\n", clientKey());
         }

         clientNode->SetData(newClientPayload, mtdps, false);
         return B_NO_ERROR;
      }
      break;

      case UNDOSTACK_COMMAND_UNDO:
printf("UndoStackMessageTreeDatabaseObject::SeniorMessageTreeUpdate():  TODO:  Implement handler for UNDOSTACK_COMMAND_UNDO\n");
         return B_UNIMPLEMENTED;
      break;

      case UNDOSTACK_COMMAND_REDO:
printf("UndoStackMessageTreeDatabaseObject::SeniorMessageTreeUpdate():  TODO:  Implement handler for UNDOSTACK_COMMAND_REDO\n");
         return B_UNIMPLEMENTED;
      break;

      default:
         return MessageTreeDatabaseObject::SeniorMessageTreeUpdate(msg);
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

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
static const String UNDOSTACK_NAME_STARTDBID       = "sid";
static const String UNDOSTACK_NAME_ENDDBID         = "eid";

static const String UNDOSTACK_NODENAME_UNDO = "undo";
static const String UNDOSTACK_NODENAME_REDO = "redo";
static const String UNDOSTACK_NODENAME_TOP  = "top";


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

status_t UndoStackMessageTreeDatabaseObject :: SeniorMessageTreeUpdate(const ConstMessageRef & msg)
{
   NestCountGuard ncg(_seniorMessageTreeUpdateNestCount);

   const status_t ret = SeniorMessageTreeUpdateAux(msg);
   if (ret.IsOK())
   {
      MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();

      // After a successful database-update, we want to also remove any undo-operations that are no longer
      // possible because the database transactions they reference are no longer present in the db-transaction-log
      DataNode * undoNode = mtdps->GetDataNode(GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO);
      if (undoNode)
      {
         const uint64 curDBID = GetCurrentDatabaseStateID()+1; // +1 because the db transaction we are finishing up here hasn't been included in the database yet
         for (DataNodeRefIterator perClientIter(undoNode->GetChildIterator()); perClientIter.HasData(); perClientIter++)
         {
            const String & nextPerClientKey = *perClientIter.GetKey();
            DataNodeRef nextPerClientNode   = perClientIter.GetValue();
            const Queue<DataNodeRef> * perClientIndex = nextPerClientNode()->GetIndex();
            if (perClientIndex)
            {
               while(perClientIndex->HasItems())
               {
                  DataNodeRef firstSeqNode = perClientIndex->Head();
                  const uint64 seqStartID  = firstSeqNode()->GetData()()->GetInt64(UNDOSTACK_NAME_STARTDBID);
                  if ((seqStartID != curDBID)&&(UpdateLogContainsUpdate(seqStartID) == false)) (void) nextPerClientNode()->RemoveChild(firstSeqNode()->GetNodeName(), mtdps, true, NULL);
                                                                                          else break;
               }
            }

            // Also remove any client-data-nodes that no longer have any children (just to be tidy)
            if ((perClientIndex==NULL)||(perClientIndex->IsEmpty())) (void) undoNode->RemoveChild(nextPerClientKey, mtdps, true, NULL);
         }
      }
   }
   return ret;
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorMessageTreeUpdateAux(const ConstMessageRef & msg)
{
   switch(msg()->what)
   {
      case UNDOSTACK_COMMAND_BEGINSEQUENCE: case UNDOSTACK_COMMAND_ENDSEQUENCE:
      {
         const String & clientKey   = *(msg()->GetStringPointer(UNDOSTACK_NAME_UNDOKEY, &GetEmptyString()));
         const String nodePath      = GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO + "/" + (clientKey.HasChars() ? clientKey : "default");
         const bool isBeginSequence = (msg()->what == UNDOSTACK_COMMAND_BEGINSEQUENCE);

         status_t ret;
         MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();
         DataNode * clientNode = mtdps->GetDataNode(nodePath);
         if ((clientNode == NULL)&&(isBeginSequence))
         {
            if (mtdps->SetDataNode(nodePath, GetMessageFromPool(0)).IsError(ret)) return ret;  // demand-create an undo-status-node for this client
            clientNode = mtdps->GetDataNode(nodePath);
         }
         if (clientNode == NULL) return B_BAD_ARGUMENT;

         const Message & oldClientPayload = *clientNode->GetData()();
         MessageRef newClientPayload;  // demand-allocated below

         if (isBeginSequence)
         {
            if (oldClientPayload.what == 0)
            {
               // Entering the first level of undo-sequence nesting -- create a new undo-sequence child node for this client
               MessageRef seqPayload = GetMessageFromPool(1);  // 1 because we've started the first level of nesting
               if (seqPayload() == NULL) RETURN_OUT_OF_MEMORY;

               const String * label = msg()->GetStringPointer(UNDOSTACK_NAME_LABEL);
               if ((label)&&(seqPayload()->CAddString(UNDOSTACK_NAME_LABEL, *label).IsError(ret))) return ret;

               const uint64 startDBID = GetCurrentDatabaseStateID()+1;  // +1 because the db transaction we are part of hasn't been included in the database yet
               if (seqPayload()->AddInt64(UNDOSTACK_NAME_STARTDBID, startDBID).IsError(ret)) return ret;

               uint32 newNodeID;
               if (mtdps->GetUnusedNodeID(nodePath, newNodeID).IsError(ret)) return ret;

               const String seqPath = nodePath + String("/I%1").Arg(newNodeID);
               if (mtdps->SetDataNode(seqPath, seqPayload, true, true, false, true).IsError(ret)) return ret;

               newClientPayload = seqPayload;  // for clients who just want to track the latest state by subscribing to their per-client node
            }
            else 
            {
               newClientPayload = GetLightweightCopyOfMessageFromPool(oldClientPayload);
               if (newClientPayload() == NULL) RETURN_OUT_OF_MEMORY;
               newClientPayload()->what++;  // increment nest-count
            }
         }
         else if (oldClientPayload.what > 1)
         {
            newClientPayload = GetLightweightCopyOfMessageFromPool(oldClientPayload);
            if (newClientPayload() == NULL) RETURN_OUT_OF_MEMORY;
            newClientPayload()->what--;  // decrement nest-count
         }
         else
         {
            const Queue<DataNodeRef> * seqIdx = clientNode->GetIndex();
            if ((seqIdx == NULL)||(seqIdx->IsEmpty())) return B_BAD_OBJECT;  // wtf?

            DataNode & seqNode = *((*seqIdx).Tail()());
            MessageRef seqPayload = GetLightweightCopyOfMessageFromPool(*seqNode.GetData()());  // make a copy as we're not allowed to modify payloads in-place
            if (seqPayload() == NULL) RETURN_OUT_OF_MEMORY;

            // Exiting the last level of undo-sequence nesting -- gotta finalize the undo-sequence child node
            const uint64 afterLastDBID = GetCurrentDatabaseStateID();  // not +1, for some reason?
            if (seqPayload()->AddInt64(UNDOSTACK_NAME_ENDDBID, afterLastDBID).IsError(ret)) return ret;

            const String * label = msg()->GetStringPointer(UNDOSTACK_NAME_LABEL);
            if ((label)&&(label->HasChars()))
            {
               (void) seqPayload()->RemoveName(UNDOSTACK_NAME_LABEL);  // to avoid updating the original/non-lightweight-shared Message
               if (seqPayload()->AddString(UNDOSTACK_NAME_LABEL, *label).IsError(ret)) return ret;
            }

            seqPayload()->what = 0;  // nest-count is now zero because we're done with this undo-sequence
            seqNode.SetData(seqPayload, mtdps, false);

            newClientPayload = seqPayload;
         }

         if (newClientPayload()) 
         {
            clientNode->SetData(newClientPayload, mtdps, false);
            return B_NO_ERROR;
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "UndoStackMessageTreeDatabaseObject:  newClientPayload() was NULL!\n");  // should never happen!
            return B_BAD_OBJECT;
         }
      }
      break;

      case UNDOSTACK_COMMAND_UNDO:
      {
         const String & clientKey   = *(msg()->GetStringPointer(UNDOSTACK_NAME_UNDOKEY, &GetEmptyString()));
         const String nodePath      = GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO + "/" + (clientKey.HasChars() ? clientKey : "default");

         MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();
         DataNode * clientNode = mtdps->GetDataNode(nodePath);
         if (clientNode)
         {
            const Queue<DataNodeRef> * indexQ = clientNode->GetIndex();
            if ((indexQ)&&(indexQ->HasItems()))
            {
               DataNodeRef undoSeq      = indexQ->Tail();
               const Message & undoData = *undoSeq()->GetData()();
               const uint64 seqStartID  = undoData.GetInt64(UNDOSTACK_NAME_STARTDBID);
               const uint64 seqEndTrans = undoData.GetInt64(UNDOSTACK_NAME_ENDDBID);
               if (seqEndTrans > seqStartID)
               {
                  // Paranoia:  first, let's make sure all of the referenced states actually exist in the transaction-log.  If they don't, then there's little point trying
                  for (uint64 transID=seqEndTrans; transID >= seqStartID; transID--)
                  {
                     if (UpdateLogContainsUpdate(transID) == false)
                     {
                        LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Can't find transaction " UINT64_FORMAT_SPEC " for client node at [%s]  (undo=" UINT64_FORMAT_SPEC " -> " UINT64_FORMAT_SPEC ")!\n", transID, nodePath(), seqEndTrans, seqStartID);
                        return B_BAD_OBJECT;
                     }
                  }

                  // Do the actual undo
                  for (uint64 transID=seqEndTrans; transID >= seqStartID; transID--)
                  {
                     ConstMessageRef payload = GetUpdatePayload(transID);
                     if (payload())
                     {
                        const String * undoKey = payload()->GetStringPointer(UNDOSTACK_NAME_UNDOKEY, &GetEmptyString());
                        if (*undoKey == clientKey)  // only undo actions that were uploaded by our own client!
                        {
                           status_t ret;
                           MessageRef undoMsg;
                           if ((payload()->FindMessage(UNDOSTACK_NAME_UNDOMESSAGE, undoMsg).IsOK())&&(SeniorMessageTreeUpdateAux(undoMsg).IsError(ret))) return ret;
                        }
                     }
                     else
                     {
                        LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Can't find transaction " UINT64_FORMAT_SPEC " for client node at [%s]  (undo=" UINT64_FORMAT_SPEC " -> " UINT64_FORMAT_SPEC ")!\n", transID, nodePath(), seqEndTrans, seqStartID);
                        return B_BAD_OBJECT;
                     }
                  }

                  return B_NO_ERROR;
               }
            }
            else LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Client node at [%s] has no undo-sequence nodes in its index!\n", nodePath());
         }
         else LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Couldn't find client node at [%s] for undo operation!\n", nodePath());

         return B_BAD_OBJECT;
      }
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
   return MessageTreeDatabaseObject::SeniorRecordNodeIndexUpdateMessage(relativePath, (op == (char)INDEX_OP_ENTRYINSERTED) ? INDEX_OP_ENTRYREMOVED : INDEX_OP_ENTRYINSERTED, index, key, _assembledJuniorUndoMessage, !prepend);
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

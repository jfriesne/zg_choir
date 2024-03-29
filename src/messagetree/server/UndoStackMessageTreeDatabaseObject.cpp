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

static const String UNDOSTACK_NODENAME_UNDO_SLASH = UNDOSTACK_NODENAME_UNDO + '/';
static const String UNDOSTACK_NODENAME_REDO_SLASH = UNDOSTACK_NODENAME_REDO + '/';

UndoStackMessageTreeDatabaseObject :: UndoStackMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath)
   : MessageTreeDatabaseObject(session, dbIndex, rootNodePath)
{
   // empty
}

status_t UndoStackMessageTreeDatabaseObject :: UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabel)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   MRETURN_OOM_ON_NULL(msg());

   MRETURN_ON_ERROR(msg()->CAddString(UNDOSTACK_NAME_LABEL, optSequenceLabel));

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

void UndoStackMessageTreeDatabaseObject :: SetToDefaultState()
{
   MessageTreeDatabaseObject::SetToDefaultState();  // clear any existing nodes

   status_t ret;
   if (SetDataNode(UNDOSTACK_NODENAME_UNDO, GetMessageFromPool()).IsError(ret)) LogTime(MUSCLE_LOG_CRITICALERROR, "UndoStackMessageTreeDatabaseObject::SetToDefaultState():  Couldn't set undo node!  [%s]\n", ret());
   if (SetDataNode(UNDOSTACK_NODENAME_REDO, GetMessageFromPool()).IsError(ret)) LogTime(MUSCLE_LOG_CRITICALERROR, "UndoStackMessageTreeDatabaseObject::SetToDefaultState():  Couldn't set redo node!  [%s]\n", ret());
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorMessageTreeUpdate(const ConstMessageRef & msg)
{
   NestCountGuard ncg(_seniorMessageTreeUpdateNestCount);

   MRETURN_ON_ERROR(SeniorMessageTreeUpdateAux(msg));

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

   return B_NO_ERROR;
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorMessageTreeUpdateAux(const ConstMessageRef & msg)
{
   switch(msg()->what)
   {
      case UNDOSTACK_COMMAND_BEGINSEQUENCE: case UNDOSTACK_COMMAND_ENDSEQUENCE:
      {
         const String & clientKey   = msg()->GetStringReference(UNDOSTACK_NAME_UNDOKEY);
         const String undoNodePath  = GetRootPathWithSlash() + UNDOSTACK_NODENAME_UNDO + "/" + (clientKey.HasChars() ? clientKey : "default");
         const bool isBeginSequence = (msg()->what == UNDOSTACK_COMMAND_BEGINSEQUENCE);

         MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();
         DataNode * clientNode = mtdps->GetDataNode(undoNodePath);
         if ((clientNode == NULL)&&(isBeginSequence))
         {
            MRETURN_ON_ERROR(mtdps->SetDataNode(undoNodePath, GetMessageFromPool(0)));  // demand-create an undo-status-node for this client
            clientNode = mtdps->GetDataNode(undoNodePath);
         }
         if (clientNode == NULL) return B_BAD_ARGUMENT;

         const Message & oldClientPayload = *clientNode->GetData()();
         MessageRef newClientPayload;  // demand-allocated below

         if (isBeginSequence)
         {
            if (oldClientPayload.what == 0)
            {
               // Begining a new undoable-sequence always clears the redo stack
               const String redoNodePath = GetRootPathWithSlash() + UNDOSTACK_NODENAME_REDO + "/" + (clientKey.HasChars() ? clientKey : "default");
               MRETURN_ON_ERROR(RemoveDataNodes(redoNodePath));

               // Entering the first level of undo-sequence nesting -- create a new undo-sequence child node for this client
               MessageRef seqPayload = GetMessageFromPool(1);  // 1 because we've started the first level of nesting
               MRETURN_OOM_ON_NULL(seqPayload());

               const String * label = msg()->GetStringPointer(UNDOSTACK_NAME_LABEL);
               if (label) MRETURN_ON_ERROR(seqPayload()->CAddString(UNDOSTACK_NAME_LABEL, *label));

               const uint64 startDBID = GetCurrentDatabaseStateID()+1;  // +1 because the db transaction we are part of hasn't been included in the database yet
               MRETURN_ON_ERROR(seqPayload()->AddInt64(UNDOSTACK_NAME_STARTDBID, startDBID));

               uint32 newNodeID;
               MRETURN_ON_ERROR(mtdps->GetUnusedNodeID(undoNodePath, newNodeID));

               const String seqPath = undoNodePath + String("/I%1").Arg(newNodeID);
               MRETURN_ON_ERROR(mtdps->SetDataNode(seqPath, seqPayload, SetDataNodeFlags(SETDATANODE_FLAG_ADDTOINDEX)));

               newClientPayload = seqPayload;  // for clients who just want to track the latest state by subscribing to their per-client node
            }
            else
            {
               newClientPayload = GetLightweightCopyOfMessageFromPool(oldClientPayload);
               MRETURN_OOM_ON_NULL(newClientPayload());
               newClientPayload()->what++;  // increment nest-count
            }
         }
         else if (oldClientPayload.what > 1)
         {
            newClientPayload = GetLightweightCopyOfMessageFromPool(oldClientPayload);
            MRETURN_OOM_ON_NULL(newClientPayload());
            newClientPayload()->what--;  // decrement nest-count
         }
         else
         {
            const Queue<DataNodeRef> * seqIdx = clientNode->GetIndex();
            if ((seqIdx == NULL)||(seqIdx->IsEmpty())) return B_BAD_OBJECT;  // wtf?

            DataNode & seqNode = *((*seqIdx).Tail()());
            MessageRef seqPayload = GetLightweightCopyOfMessageFromPool(*seqNode.GetData()());  // make a copy as we're not allowed to modify payloads in-place
            MRETURN_OOM_ON_NULL(seqPayload());

            // Exiting the last level of undo-sequence nesting -- gotta finalize the undo-sequence child node
            const uint64 afterLastDBID = GetCurrentDatabaseStateID()+1;  // +1 because the db transaction we are part of hasn't been included in the database yet
            MRETURN_ON_ERROR(seqPayload()->AddInt64(UNDOSTACK_NAME_ENDDBID, afterLastDBID));

            const String * label = msg()->GetStringPointer(UNDOSTACK_NAME_LABEL);
            if ((label)&&(label->HasChars()))
            {
               (void) seqPayload()->RemoveName(UNDOSTACK_NAME_LABEL);  // to avoid updating the original/non-lightweight-shared Message
               MRETURN_ON_ERROR(seqPayload()->AddString(UNDOSTACK_NAME_LABEL, *label));
            }

            seqPayload()->what = 0;  // nest-count is now zero because we're done with this undo-sequence
            seqNode.SetData(seqPayload, mtdps);

            newClientPayload = seqPayload;
         }

         if (newClientPayload())
         {
            clientNode->SetData(newClientPayload, mtdps);
            return B_NO_ERROR;
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "UndoStackMessageTreeDatabaseObject:  newClientPayload() was NULL!\n");  // should never happen!
            return B_BAD_OBJECT;
         }
      }
      break;

      case UNDOSTACK_COMMAND_UNDO: case UNDOSTACK_COMMAND_REDO:
      {
         const bool isRedo         = (msg()->what == UNDOSTACK_COMMAND_REDO);
         const char * desc         = isRedo ? "redo" : "undo";
         const String & clientKey  = msg()->GetStringReference(UNDOSTACK_NAME_UNDOKEY);
         const String & optOpTag   = msg()->GetStringReference(UNDOSTACK_NAME_LABEL);
         const String fromNodePath = GetRootPathWithSlash() + (isRedo ? UNDOSTACK_NODENAME_REDO : UNDOSTACK_NODENAME_UNDO) + "/" + (clientKey.HasChars() ? clientKey : "default");
         const String destNodePath = GetRootPathWithSlash() + (isRedo ? UNDOSTACK_NODENAME_UNDO : UNDOSTACK_NODENAME_REDO) + "/" + (clientKey.HasChars() ? clientKey : "default");

         DECLARE_OP_TAG_GUARD;

         MessageTreeDatabasePeerSession * mtdps = GetMessageTreeDatabasePeerSession();
         DataNode * fromClientNode = mtdps->GetDataNode(fromNodePath);
         if (fromClientNode)
         {
            const Queue<DataNodeRef> * indexQ = fromClientNode->GetIndex();
            if ((indexQ)&&(indexQ->HasItems()))
            {
               ConstMessageRef payload = indexQ->Tail()()->GetData();
               const uint64 seqStartID = payload()->GetInt64(UNDOSTACK_NAME_STARTDBID);
               const uint64 seqEndID   = payload()->GetInt64(UNDOSTACK_NAME_ENDDBID);
               if (seqEndID >= seqStartID)
               {
                  // Paranoia:  first, let's make sure all of the referenced states actually exist in the transaction-log.  If they don't, then there's little point trying
                  for (uint64 transID=seqEndID; transID >= seqStartID; transID--)
                  {
                     if (UpdateLogContainsUpdate(transID) == false)
                     {
                        LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Can't find transaction " UINT64_FORMAT_SPEC " for client node at [%s]  (%s=" UINT64_FORMAT_SPEC " -> " UINT64_FORMAT_SPEC ")!\n", transID, fromNodePath(), desc, seqEndID, seqStartID);
                        return B_BAD_OBJECT;
                     }
                  }

                  // Do the actual undo (or redo)
                  for (uint64 transID=seqEndID; transID>=seqStartID; transID--)
                  {
                     NestCountGuard ncg(_inUndoRedoContextNestCount);
                     ConstMessageRef payload = GetUpdatePayload(isRedo ? (seqStartID+(seqEndID-transID)) : transID);
                     if (payload())
                     {
                        if (payload()->GetStringReference(UNDOSTACK_NAME_UNDOKEY) == clientKey)  // only undo or redo actions that were uploaded by our own client!
                        {
                           ConstMessageRef subMsg;
                           if (payload()->FindMessage(isRedo ? UNDOSTACK_NAME_DOMESSAGE : UNDOSTACK_NAME_UNDOMESSAGE, subMsg).IsOK()) MRETURN_ON_ERROR(SeniorMessageTreeUpdateAux(subMsg));
                        }
                     }
                     else
                     {
                        LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Can't find transaction " UINT64_FORMAT_SPEC " for client node at [%s]  (%s=" UINT64_FORMAT_SPEC " -> " UINT64_FORMAT_SPEC ")!\n", transID, fromNodePath(), desc, seqEndID, seqStartID);
                        return B_BAD_OBJECT;
                     }
                  }

                  // pop the operation off of the source-stack, and delete the source-client node if the source-stack is now empty
                  MRETURN_ON_ERROR(fromClientNode->RemoveChild(indexQ->Tail()()->GetNodeName(), mtdps, true, NULL));
                  if (fromClientNode->GetNumChildren() > 0)
                  {
                     fromClientNode->SetData(indexQ->Tail()()->GetData(), mtdps);  // notify programs that are tracking the top of the source-stack
                  }
                  else (void) fromClientNode->GetParent()->RemoveChild(fromClientNode->GetNodeName(), mtdps, true, NULL);

                  // Demand-allocate a dest-client-node
                  DataNode * destClientNode = mtdps->GetDataNode(destNodePath);
                  if (destClientNode == NULL)
                  {
                     MRETURN_ON_ERROR(mtdps->SetDataNode(destNodePath, GetMessageFromPool(0)));  // demand-create a dest-status-node for this client
                     destClientNode = mtdps->GetDataNode(destNodePath);
                  }
                  if (destClientNode == NULL) return B_BAD_OBJECT;

                  // And finally, push the operation to the top of the dest-stack
                  uint32 newNodeID;
                  MRETURN_ON_ERROR(mtdps->GetUnusedNodeID(destNodePath, newNodeID));

                  const String destSeqPath = destNodePath + String("/I%1").Arg(newNodeID);
                  MRETURN_ON_ERROR(mtdps->SetDataNode(destSeqPath, payload, SetDataNodeFlags(SETDATANODE_FLAG_ADDTOINDEX)));

                  destClientNode->SetData(payload, mtdps);  // notify programs that are tracking the top of the dest-stack
                  return B_NO_ERROR;
               }
            }
            else LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Client node at [%s] has no %s-sequence nodes in its index!\n", fromNodePath(), desc);
         }
         else LogTime(MUSCLE_LOG_ERROR, "UndoStackMessageTreeDatabaseObject:  Couldn't find client node at [%s] for %s operation!\n", fromNodePath(), desc);

         return B_BAD_OBJECT;
      }
      break;

      default:
         return MessageTreeDatabaseObject::SeniorMessageTreeUpdate(msg);
   }
}

status_t UndoStackMessageTreeDatabaseObject :: JuniorUpdate(const ConstMessageRef & pairMsg)
{
   MessageRef doMsg = pairMsg()->GetMessage(UNDOSTACK_NAME_DOMESSAGE);
   return MessageTreeDatabaseObject::JuniorUpdate(doMsg() ? doMsg : pairMsg);   // if there's no doMsg, it's probably because a subclass requested a custom change
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorRecordNodeUpdateMessage(const String & relativePath, const ConstMessageRef & oldPayload, const ConstMessageRef & newPayload, MessageRef & assemblingMessage, bool prepend, const String & optOpTag)
{
   // File the do-action as usual for our Junior Peers to use
   MRETURN_ON_ERROR(MessageTreeDatabaseObject::SeniorRecordNodeUpdateMessage(relativePath, oldPayload, newPayload, assemblingMessage, prepend, optOpTag));

   // Also prepend the equal-and-opposite undo-action to the beginning of our _assembledJuniorUndoMessage (in case we ever want to undo this action later)
   return MessageTreeDatabaseObject::SeniorRecordNodeUpdateMessage(relativePath, newPayload, oldPayload, _assembledJuniorUndoMessage, !prepend, optOpTag);
}

status_t UndoStackMessageTreeDatabaseObject :: SeniorRecordNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, MessageRef & assemblingMessage, bool prepend, const String & optOpTag)
{
   // File the do-action as usual for our Junior Peers to use
   MRETURN_ON_ERROR(MessageTreeDatabaseObject::SeniorRecordNodeIndexUpdateMessage(relativePath, op, index, key, assemblingMessage, prepend, optOpTag));

   // Also prepend the equal-and-opposite undo-action to the beginning of our _assembledJuniorUndoMessage (in case we ever want to undo this action later)
   return MessageTreeDatabaseObject::SeniorRecordNodeIndexUpdateMessage(relativePath, (op == (char)INDEX_OP_ENTRYINSERTED) ? INDEX_OP_ENTRYREMOVED : INDEX_OP_ENTRYINSERTED, index, key, _assembledJuniorUndoMessage, !prepend, optOpTag);
}

status_t UndoStackMessageTreeDatabaseObject :: RequestReplaceDatabaseState(const MessageRef & newDatabaseStateMsg)
{
   MRETURN_ON_ERROR(newDatabaseStateMsg()->CAddString(UNDOSTACK_NAME_UNDOKEY, GetActiveClientUndoKey()));
   return MessageTreeDatabaseObject::RequestReplaceDatabaseState(newDatabaseStateMsg);
}

status_t UndoStackMessageTreeDatabaseObject :: RequestUpdateDatabaseState(const MessageRef & databaseUpdateMsg)
{
   MRETURN_ON_ERROR(databaseUpdateMsg()->CAddString(UNDOSTACK_NAME_UNDOKEY, GetActiveClientUndoKey()));
   return MessageTreeDatabaseObject::RequestUpdateDatabaseState(databaseUpdateMsg);
}

const String & UndoStackMessageTreeDatabaseObject :: GetActiveClientUndoKey() const
{
   const ServerSideMessageTreeSession * ssmts = GetMessageTreeDatabasePeerSession()->GetActiveServerSideMessageTreeSession();
   return ssmts ? ssmts->GetUndoKey() : GetEmptyString();
}

bool UndoStackMessageTreeDatabaseObject :: IsOkayToHandleUpdateMessage(const String & path, TreeGatewayFlags flags) const
{
   if (_inUndoRedoContextNestCount.IsInBatch() == false) return true;  // always handle everything when outside of an "undo" or "redo" op

   // No need to execute interim-updates (e.g. updates generated during the middle of a mouse-drag)
   // when doing an undo or a redo, since they are intended to be idempotent wrt the updates that came
   // before or after them.
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_INTERIM)) return false;

   // Don't touch the undo or redo stacks during and undo or redo!
   return !((path.StartsWith(UNDOSTACK_NODENAME_UNDO_SLASH))||(path.StartsWith(UNDOSTACK_NODENAME_REDO_SLASH)));
}

}; // end namespace zg

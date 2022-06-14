#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"
#include "zg/messagetree/gateway/SymlinkLogicMuxTreeGateway.h"  // just for SYMLINK_FIELD_NAME
#include "reflector/StorageReflectSession.h"  // for NODE_DEPTH_USER
#include "regex/SegmentedStringMatcher.h"
#include "util/MiscUtilityFunctions.h"  // for AssembleBatchMessage()

namespace zg 
{

// Command-codes that can be used in both SeniorUpdate() and JuniorUpdate()
enum {
   MTDO_COMMAND_NOOP = 1836344163, // 'mtcc' 
   MTDO_COMMAND_UPDATENODEVALUE,
   MTDO_COMMAND_UPDATESUBTREE,
   MTDO_COMMAND_INSERTINDEXENTRY,
   MTDO_COMMAND_REMOVEINDEXENTRY,
};

// Command-codes that can be used only in SeniorUpdate()
enum {
   MTDO_SENIOR_COMMAND_REQUESTDELETENODES = 1836348259, // 'mtsc' 
   MTDO_SENIOR_COMMAND_MOVEINDEXENTRY,
};

// Command-codes that can be used only in JuniorUpdate()
enum {
   MTDO_JUNIOR_COMMAND_UNUSED = 1836345955, // 'mtjc' 
};

static const String MTDO_NAME_PATH    = "pth";
static const String MTDO_NAME_PAYLOAD = "pay";
static const String MTDO_NAME_FLAGS   = "flg";
static const String MTDO_NAME_BEFORE  = "be4";
static const String MTDO_NAME_FILTER  = "fil";
static const String MTDO_NAME_INDEX   = "idx";
static const String MTDO_NAME_KEY     = "key";
static const String MTDO_NAME_TAG     = "tag";

MessageTreeDatabaseObject :: MessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : IDatabaseObject(session, dbIndex)
   , _rootNodePathWithoutSlash(rootNodePath.WithoutSuffix("/"))
   , _rootNodePathWithSlash(rootNodePath.WithSuffix("/"))
   , _rootNodeDepth(GetPathDepth(rootNodePath()))
   , _checksum(0)
{
   // empty
}

void MessageTreeDatabaseObject :: SetToDefaultState()
{
   (void) RemoveDataNodes(_rootNodePathWithoutSlash);
}

status_t MessageTreeDatabaseObject :: SetFromArchive(const ConstMessageRef & archive)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   return zsh->RestoreNodeTreeFromMessage(*archive(), _rootNodePathWithoutSlash, true);
}

status_t MessageTreeDatabaseObject :: SaveToArchive(const MessageRef & archive) const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   const DataNode * rootNode = zsh->GetDataNode(_rootNodePathWithoutSlash);
   return rootNode ? zsh->SaveNodeTreeToMessage(*archive(), rootNode, GetEmptyString(), true) : B_NO_ERROR;
}

uint32 MessageTreeDatabaseObject :: CalculateChecksum() const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return 0;

   const DataNode * rootNode = zsh->GetDataNode(_rootNodePathWithoutSlash);
   return rootNode ? rootNode->CalculateChecksum() : 0;
}

ConstMessageRef MessageTreeDatabaseObject :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   GatewaySubscriberCommandBatchGuard<ITreeGateway> batchGuard(GetMessageTreeDatabasePeerSession());  // so that MessageTreeDatabasePeerSession::CommandBatchEnds() will call PushSubscriptionMessages() when we're done

   const status_t ret = SeniorMessageTreeUpdate(seniorDoMsg);
   if (ret.IsError())
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject::SeniorUpdate():  SeniorMessageTreeUpdate() failed! [%s]\n", ret());
      return ConstMessageRef();
   }

   if (_assembledJuniorMessage() == NULL) _assembledJuniorMessage = GetMessageFromPool(MTDO_COMMAND_NOOP);

   ConstMessageRef juniorMsg = _assembledJuniorMessage;
   _assembledJuniorMessage.Reset();
   return juniorMsg;
}

String MessageTreeDatabaseObject :: DatabaseSubpathToSessionRelativePath(const String & subPath, TreeGatewayFlags flags) const
{
   const String ret = subPath.HasChars() ? _rootNodePathWithoutSlash.AppendWord(subPath, "/") : _rootNodePathWithoutSlash;
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_TRAVERSE_SYMLINK))
   {
      const DataNode * dn = GetDataNode(subPath);
      const String * symlinkTarget = dn ? dn->GetData()()->GetStringPointer(SYMLINK_FIELD_NAME) : NULL;
      if (symlinkTarget) return *symlinkTarget;
   }
   return ret;
}

status_t MessageTreeDatabaseObject :: SeniorMessageTreeUpdate(const ConstMessageRef & msg)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   switch(msg()->what)
   {
      case PR_COMMAND_BATCH:
      {
         MessageRef subMsg;
         for (int32 i=0; msg()->FindMessage(PR_NAME_KEYS, i, subMsg).IsOK(); i++) MRETURN_ON_ERROR(SeniorMessageTreeUpdate(subMsg));
      }
      break;

      case MTDO_COMMAND_NOOP:
         // empty
      break;

      case MTDO_COMMAND_UPDATENODEVALUE:
         return HandleNodeUpdateMessage(*msg());
      break;

      case MTDO_COMMAND_UPDATESUBTREE:
         return HandleSubtreeUpdateMessage(*msg());
      break;

      case MTDO_COMMAND_INSERTINDEXENTRY:
      case MTDO_COMMAND_REMOVEINDEXENTRY:
         return HandleNodeIndexUpdateMessage(*msg());
      break;

      case MTDO_SENIOR_COMMAND_REQUESTDELETENODES:
      {
         MessageRef qfMsg;
         const TreeGatewayFlags flags = msg()->GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
         const String & path          = msg()->GetStringReference(MTDO_NAME_PATH);
         const String & optOpTag      = msg()->GetStringReference(MTDO_NAME_TAG);
         ConstQueryFilterRef qfRef    = msg()->FindMessage(MTDO_NAME_FILTER, qfMsg).IsOK() ? GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg()) : QueryFilterRef();

         return RemoveDataNodes(DatabaseSubpathToSessionRelativePath(path, flags), qfRef, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY), optOpTag);
      }
      break;

      case MTDO_SENIOR_COMMAND_MOVEINDEXENTRY:
      {
         MessageRef qfMsg;
         const TreeGatewayFlags flags = msg()->GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
         const String & path          = msg()->GetStringReference(MTDO_NAME_PATH);
         const String & optBefore     = msg()->GetStringReference(MTDO_NAME_BEFORE);
         const String & optOpTag      = msg()->GetStringReference(MTDO_NAME_TAG);
         ConstQueryFilterRef qfRef    = msg()->FindMessage(MTDO_NAME_FILTER, qfMsg).IsOK() ? GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg()) : QueryFilterRef();

         return MoveIndexEntries(DatabaseSubpathToSessionRelativePath(path, flags), optBefore, qfRef, optOpTag);
      }
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject::SeniorMessageTreeUpdate():  Unknown Message code " UINT32_FORMAT_SPEC "\n", msg()->what);
         return B_UNIMPLEMENTED;
   }

   return B_NO_ERROR;
}

status_t MessageTreeDatabaseObject :: JuniorUpdate(const ConstMessageRef & juniorDoMsg)
{
   GatewaySubscriberCommandBatchGuard<ITreeGateway> batchGuard(GetMessageTreeDatabasePeerSession());  // so that MessageTreeDatabasePeerSession::CommandBatchEnds() will call PushSubscriptionMessages() when we're done

   status_t ret;
   if (JuniorMessageTreeUpdate(juniorDoMsg).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject::JuniorUpdate():  JuniorUpdate() failed! [%s]\n", ret());
      return ret;
   }

   return B_NO_ERROR;
}

status_t MessageTreeDatabaseObject :: JuniorMessageTreeUpdate(const ConstMessageRef & msg)
{
   switch(msg()->what)
   {
      case PR_COMMAND_BATCH:
      {
         MessageRef subMsg;
         for (int32 i=0; msg()->FindMessage(PR_NAME_KEYS, i, subMsg).IsOK(); i++) MRETURN_ON_ERROR(JuniorMessageTreeUpdate(subMsg));
      }
      break;

      case MTDO_COMMAND_NOOP:
         // empty
      break;

      case MTDO_COMMAND_UPDATENODEVALUE:
         return HandleNodeUpdateMessage(*msg());
      break;

      case MTDO_COMMAND_UPDATESUBTREE:
         LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeDatabaseObject::JuniorMessageTreeUpdate():  MTDO_COMMAND_UPDATESUBTREE shouldn't be used in a junior-peer context!\n");
      return B_BAD_ARGUMENT;

      case MTDO_COMMAND_INSERTINDEXENTRY:
      case MTDO_COMMAND_REMOVEINDEXENTRY:
         return HandleNodeIndexUpdateMessage(*msg());
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject::JuniorMessageTreeUpdate():  Unknown Message code " UINT32_FORMAT_SPEC "\n", msg()->what);
         msg()->PrintToStream();
      return B_UNIMPLEMENTED;
   }

   return B_NO_ERROR;
}

bool MessageTreeDatabaseObject :: IsInSetupOrTeardown() const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return ((zsh)&&(zsh->IsInSetupOrTeardown()));
}

void MessageTreeDatabaseObject :: MessageTreeNodeUpdated(const String & relativePath, DataNode & node, const MessageRef & oldPayload, bool isBeingRemoved)
{
   if (IsInSeniorDatabaseUpdateContext())
   {
      const status_t ret = SeniorRecordNodeUpdateMessage(relativePath, oldPayload, isBeingRemoved?MessageRef():node.GetData(), _assembledJuniorMessage, false, GetCurrentOpTag());
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeNodeUpdated %p:  Error assembling junior message for %s node [%s]!  [%s]\n", this, isBeingRemoved?"removed":"updated", relativePath(), ret());
   }
   else if ((IsInJuniorDatabaseUpdateContext() == false)&&(IsInSetupOrTeardown() == false))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeNodeUpdated %p:  node [%s] was %s outside of either senior or junior update context of database [%s]!\n", this, relativePath(), isBeingRemoved?"removed":"updated", _rootNodePathWithoutSlash());
      PrintStackTrace();
   }

   // Update our running database-checksum to account for the changes being made to our subtree
        if (isBeingRemoved) _checksum -= node.CalculateChecksum();
   else if (oldPayload())
   {
      _checksum -= oldPayload()->CalculateChecksum();
      if (node.GetData()()) _checksum += node.GetData()()->CalculateChecksum();
   }
   else _checksum += node.CalculateChecksum();
}

status_t MessageTreeDatabaseObject :: SeniorRecordNodeUpdateMessage(const String & relativePath, const MessageRef & /*oldPayload*/, const MessageRef & newPayload, MessageRef & assemblingMessage, bool prepend, const String & optOpTag)
{
   MessageRef msg = CreateNodeUpdateMessage(relativePath, newPayload, _interimUpdateNestCount.IsInBatch()?TreeGatewayFlags(TREE_GATEWAY_FLAG_INTERIM):TreeGatewayFlags(), GetEmptyString(), optOpTag);
   MRETURN_OOM_ON_NULL(msg());

   return AssembleBatchMessage(assemblingMessage, msg, prepend);
}

void MessageTreeDatabaseObject :: MessageTreeNodeIndexChanged(const String & relativePath, DataNode & /*node*/, char op, uint32 index, const String & key)
{
   if (IsInSeniorDatabaseUpdateContext())
   {
      const status_t ret = SeniorRecordNodeIndexUpdateMessage(relativePath, op, index, key, _assembledJuniorMessage, false, GetCurrentOpTag());
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeNodeIndexChanged %p:  Error assembling junior message for node-index-update to [%s]!  [%s]\n", this, relativePath(), ret());
   }
   else if ((IsInJuniorDatabaseUpdateContext() == false)&&(IsInSetupOrTeardown() == false))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeNodeIndexChanged %p:  index for node [%s] was updated outside of either senior or junior update context of database [%s]\n", this, relativePath(), _rootNodePathWithoutSlash());
      PrintStackTrace();
   }

   // Update our running database-checksum to account for the changes being made to our subtree
   switch(op)
   {
      case INDEX_OP_ENTRYINSERTED: _checksum += key.CalculateChecksum(); break;
      case INDEX_OP_ENTRYREMOVED:  _checksum -= key.CalculateChecksum(); break;
      case INDEX_OP_CLEARED:       LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeNodeIndexChanged():  checksum-update for INDEX_OP_CLEARED is not implemented!  (%s)\n", relativePath()); break;  // Dunno how to handle this, and it never gets called anyway
   }
}

status_t MessageTreeDatabaseObject :: SeniorRecordNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, MessageRef & assemblingMessage, bool prepend, const String & optOpTag)
{
   MessageRef msg = CreateNodeIndexUpdateMessage(relativePath, op, index, key, optOpTag);
   MRETURN_OOM_ON_NULL(msg());

   return AssembleBatchMessage(assemblingMessage, msg, prepend);
}

String MessageTreeDatabaseObject :: ToString() const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return "<no database peer session!>";

   String ret;
   const DataNode * rootNode = zsh->GetDataNode(_rootNodePathWithoutSlash);
   if (rootNode) DumpDescriptionToString(*rootNode, ret, 0);
   return ret;
}

void MessageTreeDatabaseObject :: DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const
{
   s += node.GetNodePath().Pad(indentLevel);

   const Message * msg = node.GetData()();
   s += String(" (msg->what=%1, #Fields=%2, FlattenedSize=%3, Checksum=%4 #children=%5, indexSize=%6)\n").Arg(msg?msg->what:666).Arg(msg?msg->GetNumNames():666).Arg(msg?msg->FlattenedSize():666).Arg(msg?msg->CalculateChecksum():666).Arg(node.GetNumChildren()).Arg(node.GetIndex()?node.GetIndex()->GetNumItems():0);
   for (DataNodeRefIterator dnIter(node.GetChildIterator()); dnIter.HasData(); dnIter++) DumpDescriptionToString(*dnIter.GetValue()(), s, indentLevel+2);
}

status_t MessageTreeDatabaseObject :: UploadNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag)
{
   MessageRef cmdMsg = CreateNodeUpdateMessage(path, optPayload, flags, optBefore, optOpTag);
   return cmdMsg() ? RequestUpdateDatabaseState(cmdMsg) : B_OUT_OF_MEMORY;
}

status_t MessageTreeDatabaseObject :: UploadNodeSubtree(const String & path, const MessageRef & valuesMsg, TreeGatewayFlags flags, const String & optOpTag)
{
   MessageRef cmdMsg = CreateSubtreeUpdateMessage(path, valuesMsg, flags, optOpTag);
   return cmdMsg() ? RequestUpdateDatabaseState(cmdMsg) : B_OUT_OF_MEMORY;
}

status_t MessageTreeDatabaseObject :: RequestDeleteNodes(const String & path, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags, const String & optOpTag)
{
   MessageRef cmdMsg = GetMessageFromPool(MTDO_SENIOR_COMMAND_REQUESTDELETENODES);
   MRETURN_OOM_ON_NULL(cmdMsg());

   if (optFilter()) MRETURN_ON_ERROR(cmdMsg()->AddArchiveMessage(MTDO_NAME_FILTER, *optFilter()));

   MRETURN_ON_ERROR(cmdMsg()->CAddString(MTDO_NAME_PATH,  path));
   MRETURN_ON_ERROR(cmdMsg()->CAddString(MTDO_NAME_TAG,   optOpTag));
   MRETURN_ON_ERROR(cmdMsg()->AddFlat(   MTDO_NAME_FLAGS, flags));
   return RequestUpdateDatabaseState(cmdMsg);
}

status_t MessageTreeDatabaseObject :: RequestMoveIndexEntry(const String & path, const String & optBefore, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags, const String & optOpTag)
{
   MessageRef cmdMsg = GetMessageFromPool(MTDO_SENIOR_COMMAND_MOVEINDEXENTRY);
   MRETURN_OOM_ON_NULL(cmdMsg());

   if (optFilter()) MRETURN_ON_ERROR(cmdMsg()->AddArchiveMessage(MTDO_NAME_FILTER, *optFilter()));

   MRETURN_ON_ERROR(cmdMsg()->CAddString(MTDO_NAME_PATH,   path));
   MRETURN_ON_ERROR(cmdMsg()->CAddString(MTDO_NAME_TAG,    optOpTag));
   MRETURN_ON_ERROR(cmdMsg()->AddFlat(   MTDO_NAME_FLAGS,  flags));
   MRETURN_ON_ERROR(cmdMsg()->CAddString(MTDO_NAME_BEFORE, optBefore));
   return RequestUpdateDatabaseState(cmdMsg);
}

int32 MessageTreeDatabaseObject :: GetDatabaseSubpath(const String & path, String * optRetRelativePath) const
{
        if (path.StartsWith('/')) return GetDatabaseSubpath(GetPathClause(NODE_DEPTH_USER, path()), optRetRelativePath);   // convert absolute path to session-relative path
   else if (CanWildcardStringMatchMultipleValues(path()))
   {
      // Gotta check the first (_rootNodeDepth) segments of (path) to see if their wildcards can match our path
      const uint32 pathDepth = GetPathDepth(path());
      if (pathDepth < _rootNodeDepth) return -1;  // (path) is too short to reach our sub-tree anyway

      const SegmentedStringMatcher ssm(path, true, "/", _rootNodeDepth);
      if (ssm.Match(_rootNodePathWithoutSlash))
      {
         if (optRetRelativePath) *optRetRelativePath = GetPathClause(_rootNodeDepth, path());
         return (pathDepth-_rootNodeDepth);
      }
      else return -1; 
   }
   else if (path == _rootNodePathWithoutSlash)
   {
      if (optRetRelativePath) optRetRelativePath->Clear();
      return 0; 
   }
   else if ((_rootNodePathWithoutSlash.IsEmpty())||((path.StartsWith(_rootNodePathWithSlash))))
   {
      String temp = _rootNodePathWithoutSlash.IsEmpty() ? path : path.Substring(_rootNodePathWithSlash.Length());
      const uint32 ret = temp.GetNumInstancesOf('/')+1;
      if (optRetRelativePath) 
      {
         optRetRelativePath->SwapContents(temp);
         if ((path.EndsWith('/'))&&(!optRetRelativePath->EndsWith('/'))) *optRetRelativePath += '/';  // for when the user is requesting a new node ID in the db-subtree-root
      }
      return ret;
   }
   else return -1;
}

// Creates MTDO_COMMAND_UPDATENODEVALUE Messages
MessageRef MessageTreeDatabaseObject :: CreateNodeUpdateMessage(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag) const
{
   MessageRef cmdMsg = GetMessageFromPool(MTDO_COMMAND_UPDATENODEVALUE);
   if (cmdMsg())
   {
      const status_t ret = cmdMsg()->CAddString( MTDO_NAME_PATH,     path)
                         | cmdMsg()->CAddMessage(MTDO_NAME_PAYLOAD,  optPayload)
                         | cmdMsg()->AddFlat(    MTDO_NAME_FLAGS,    flags)
                         | cmdMsg()->CAddString( MTDO_NAME_BEFORE,   optBefore)
                         | cmdMsg()->CAddString( MTDO_NAME_TAG,      optOpTag);
      if (ret.IsOK()) return cmdMsg;
   }

   LogTime(MUSCLE_LOG_CRITICALERROR, "Error assembling node-update Message for %s -> %p\n", path(), optPayload());
   return MessageRef();
}

// Creates MTDO_COMMAND_UPDATESUBTREE Messages
MessageRef MessageTreeDatabaseObject :: CreateSubtreeUpdateMessage(const String & path, const MessageRef & payload, TreeGatewayFlags flags, const String & optOpTag) const
{
   MessageRef cmdMsg = GetMessageFromPool(MTDO_COMMAND_UPDATESUBTREE);
   if (cmdMsg())
   {
      const status_t ret = cmdMsg()->CAddString(MTDO_NAME_PATH,    path)
                         | cmdMsg()->AddMessage(MTDO_NAME_PAYLOAD, payload)
                         | cmdMsg()->AddFlat(   MTDO_NAME_FLAGS,   flags)
                         | cmdMsg()->CAddString(MTDO_NAME_TAG,     optOpTag);
      if (ret.IsOK()) return cmdMsg;
   }

   LogTime(MUSCLE_LOG_CRITICALERROR, "Error assembling subtree-update Message for %s -> %p\n", path(), payload());
   return MessageRef();
}

MessageRef MessageTreeDatabaseObject :: CreateNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, const String & optOpTag)
{
   uint32 whatCode = 0;
   switch(op)
   {
      case INDEX_OP_ENTRYINSERTED: whatCode = MTDO_COMMAND_INSERTINDEXENTRY; break;
      case INDEX_OP_ENTRYREMOVED:  whatCode = MTDO_COMMAND_REMOVEINDEXENTRY; break;
      default:                     LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeNodeIndexChanged %p:  Unknown opCode %c for path [%s]\n", this, op, relativePath()); break;
   }

   if (whatCode != 0)
   {
      // update our assembled-junior-message so the junior-peers can later replicate what we did here
      MessageRef juniorMsg = GetMessageFromPool(whatCode);
      if (juniorMsg())
      {
         const status_t ret = juniorMsg()->CAddString(MTDO_NAME_PATH,  relativePath)
                            | juniorMsg()->CAddInt32( MTDO_NAME_INDEX, index)
                            | juniorMsg()->CAddString(MTDO_NAME_KEY,   key)
                            | juniorMsg()->CAddString(MTDO_NAME_TAG,   optOpTag);
         if (ret.IsOK()) return juniorMsg;
      }
   }
   return MessageRef();
}

// Handles MTDO_COMMAND_UPDATENODEVALUE Messages
status_t MessageTreeDatabaseObject :: HandleNodeUpdateMessage(const Message & msg)
{
   const TreeGatewayFlags flags = msg.GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
   if (IsOkayToHandleUpdateMessage(msg.GetString(MTDO_NAME_PATH), flags) == false) return B_NO_ERROR;

   const bool isInterimUpdate = flags.IsBitSet(TREE_GATEWAY_FLAG_INTERIM);
   if (isInterimUpdate) _interimUpdateNestCount.Increment();
   const status_t ret = HandleNodeUpdateMessageAux(msg, flags);
   if (isInterimUpdate) _interimUpdateNestCount.Decrement();

   return ret;
}

#define DECLARE_OP_TAG_GUARD const OpTagGuard tagGuard(optOpTag, this)

status_t MessageTreeDatabaseObject :: HandleNodeUpdateMessageAux(const Message & msg, TreeGatewayFlags flags)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();

   MessageRef optPayload        = msg.GetMessage(MTDO_NAME_PAYLOAD);
   const String & path          = msg.GetStringReference(MTDO_NAME_PATH);
   const String & optOpTag      = msg.GetStringReference(MTDO_NAME_TAG);
 
   DECLARE_OP_TAG_GUARD;

   if (optPayload())
   {
      const String & optBefore   = msg.GetStringReference(MTDO_NAME_BEFORE);
      String sessionRelativePath = DatabaseSubpathToSessionRelativePath(path, flags);
      if ((IsInSeniorDatabaseUpdateContext())&&(sessionRelativePath.EndsWith('/')))
      {
         // Client wants us to choose an available node ID
         sessionRelativePath--;

         uint32 newNodeID;
         MRETURN_ON_ERROR(zsh->GetUnusedNodeID(sessionRelativePath, newNodeID));

         char buf[64]; muscleSprintf(buf, "/%s" UINT32_FORMAT_SPEC, flags.IsBitSet(TREE_GATEWAY_FLAG_INDEXED)?"I":"", newNodeID);
         sessionRelativePath += buf;
      }

//printf("   SetDataNode [%s] -> %p (%s) (flags=%s optBefore=[%s])\n", sessionRelativePath(), optPayload(), flags.ToHexString()(), flags.ToHexString()(), optBefore());
      SetDataNodeFlags sdnFlags;
      const status_t ret = zsh->SetDataNode(sessionRelativePath, optPayload, ConvertTreeGatewayFlagsToSetDataNodeFlags(flags), optBefore.HasChars()?&optBefore:NULL);
      return ((ret.IsOK())||((ret == B_ACCESS_DENIED)&&(flags.AreAnyOfTheseBitsSet(TREE_GATEWAY_FLAG_DONTCREATENODE, TREE_GATEWAY_FLAG_DONTOVERWRITEDATA)))) ? B_NO_ERROR : ret;
   }
   else return RemoveDataNodes(DatabaseSubpathToSessionRelativePath(path, flags), ConstQueryFilterRef(), flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
}

// Handles MTDO_COMMAND_INSERTINDEXENTRY and MTDO_COMMAND_REMOVEINDEXENTRY Messages
status_t MessageTreeDatabaseObject :: HandleNodeIndexUpdateMessage(const Message & msg)
{
   const String &          path = msg.GetStringReference(MTDO_NAME_PATH);
   const TreeGatewayFlags flags = msg.GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
   if (IsOkayToHandleUpdateMessage(path, TreeGatewayFlags()) == false) return B_NO_ERROR;

   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   const String sessionRelativePath = DatabaseSubpathToSessionRelativePath(path, flags);
   DataNode * node = zsh->GetDataNode(sessionRelativePath);
   if (node) 
   {
      const String & key      = msg.GetStringReference(MTDO_NAME_KEY);
      const String & optOpTag = msg.GetStringReference(MTDO_NAME_TAG);
      const int32 index       = msg.GetInt32(          MTDO_NAME_INDEX);

      DECLARE_OP_TAG_GUARD;

      if (msg.what == MTDO_COMMAND_INSERTINDEXENTRY) node->InsertIndexEntryAt(index, zsh, key);
                                                else node->RemoveIndexEntryAt(index, zsh);
//printf("   %s (path=[%s]) index=%u key=[%s] indexLength=%u\n", (msg.what == MTDO_COMMAND_INSERTINDEXENTRY)?"INSERT":"REMOVE", sessionRelativePath(), index, key?key->Cstr():NULL, node?node->GetIndex()->GetNumItems():666);
      return B_NO_ERROR;
   }
   else 
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "HandleNodeIndexUpdateMessage:  Couldn't find node for path [%s] to update node-index!\n", sessionRelativePath());
      return B_DATA_NOT_FOUND;
   }
}

// Handles MTDO_COMMAND_UPDATESUBTREE Messages
status_t MessageTreeDatabaseObject :: HandleSubtreeUpdateMessage(const Message & msg)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();

   MessageRef payload  = msg.GetMessage(MTDO_NAME_PAYLOAD);
   const String & path = msg.GetStringReference(MTDO_NAME_PATH);
   if (payload())
   {
      const TreeGatewayFlags flags = msg.GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
      const String & optOpTag = msg.GetStringReference(MTDO_NAME_TAG);

      DECLARE_OP_TAG_GUARD;

      SetDataNodeFlags sdnFlags;
#ifdef JAF_DELIBERATELY_REMOVED_THIS_LINE_BECAUSE_IT_PREVENTS_DATA_REPLICATION_FROM_WORKING_ACROSS_PEERS
      if (flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY)) sdnFlags.SetBit(SETDATANODE_FLAG_QUIET);
#endif
      return zsh->RestoreNodeTreeFromMessage(*payload(), DatabaseSubpathToSessionRelativePath(path, flags), true, sdnFlags);
   }
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "HandleSubtreeUpdateMessage():  No payload found for path [%s]\n", path());
      return B_BAD_ARGUMENT;
   }
}

bool MessageTreeDatabaseObject :: IsNodeInThisDatabase(const DataNode & dn) const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return ((zsh)&&(zsh->GetDatabaseForNodePath(dn.GetNodePath(), NULL) == this));
}

// Like StorageReflectSession::RemoveDataNodes(), except it is careful not to remove data nodes that aren't part of our own database
status_t MessageTreeDatabaseObject :: RemoveDataNodes(const String & nodePath, const ConstQueryFilterRef & filterRef, bool quiet, const String & optOpTag)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   DECLARE_OP_TAG_GUARD;

   const SafeQueryFilter safeQF(this);
   AndQueryFilter andQF = AndQueryFilter(DummyConstQueryFilterRef(safeQF));
   if (filterRef()) (void) andQF.GetChildren().AddTail(filterRef);
   return zsh->RemoveDataNodes(nodePath, DummyConstQueryFilterRef(andQF), quiet);
}

// Like StorageReflectSession::MoveIndexEntries(), except it is careful not to modify the indices of any data nodes that aren't part of our own database
status_t MessageTreeDatabaseObject :: MoveIndexEntries(const String & nodePath, const String & optBefore, const ConstQueryFilterRef & filterRef, const String & optOpTag)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   DECLARE_OP_TAG_GUARD;

   const SafeQueryFilter safeQF(this);
   AndQueryFilter andQF = AndQueryFilter(DummyConstQueryFilterRef(safeQF));
   if (filterRef()) (void) andQF.GetChildren().AddTail(filterRef);
   return zsh->MoveIndexEntries(nodePath, optBefore.HasChars()?&optBefore:NULL, DummyConstQueryFilterRef(andQF));
}

MessageTreeDatabasePeerSession * MessageTreeDatabaseObject :: GetMessageTreeDatabasePeerSession() const 
{
   return static_cast<MessageTreeDatabasePeerSession *>(GetDatabasePeerSession());
}

DataNode * MessageTreeDatabaseObject :: GetDataNode(const String & nodePath) const
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->GetDataNode(nodePath.StartsWith("/") ? nodePath : DatabaseSubpathToSessionRelativePath(nodePath, TreeGatewayFlags())) : NULL;
}

status_t MessageTreeDatabaseObject :: SetDataNode(const String & nodePath, const MessageRef & dataMsgRef, SetDataNodeFlags flags, const String & optInsertBefore, const String & optOpTag)
{
   DECLARE_OP_TAG_GUARD;

   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->SetDataNode(DatabaseSubpathToSessionRelativePath(nodePath, TreeGatewayFlags()), dataMsgRef, flags, optInsertBefore.HasChars()?&optInsertBefore:NULL) : B_BAD_OBJECT;
}

status_t MessageTreeDatabaseObject :: FindMatchingNodes(const String & nodePath, const ConstQueryFilterRef & filter, Queue<DataNodeRef> & retMatchingNodes, uint32 maxResults) const
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->FindMatchingNodes(nodePath.StartsWith("/") ? nodePath : DatabaseSubpathToSessionRelativePath(nodePath, TreeGatewayFlags()), filter, retMatchingNodes, maxResults) : B_BAD_OBJECT;
}

DataNodeRef MessageTreeDatabaseObject :: FindMatchingNode(const String & nodePath, const ConstQueryFilterRef & filter) const
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->FindMatchingNode(nodePath.StartsWith("/") ? nodePath : DatabaseSubpathToSessionRelativePath(nodePath, TreeGatewayFlags()), filter) : DataNodeRef();
}

status_t MessageTreeDatabaseObject :: SaveNodeTreeToMessage(Message & msg, const DataNode & node, const String & path, bool saveData, uint32 maxDepth, const ITraversalPruner * optPruner) const
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->SaveNodeTreeToMessage(msg, &node, path, saveData, maxDepth, optPruner) : B_BAD_OBJECT;
}

status_t MessageTreeDatabaseObject :: RestoreNodeTreeFromMessage(const Message & msg, const String & path, bool loadData, SetDataNodeFlags flags, uint32 maxDepth, const ITraversalPruner * optPruner, const String & optOpTag)
{
   DECLARE_OP_TAG_GUARD;

   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->RestoreNodeTreeFromMessage(msg, DatabaseSubpathToSessionRelativePath(path, TreeGatewayFlags()), loadData, flags, maxDepth, optPruner) : B_BAD_OBJECT;
}

status_t MessageTreeDatabaseObject :: CloneDataNodeSubtree(const DataNode & sourceNode, const String & destPath, SetDataNodeFlags flags, const String * optInsertBefore, const ITraversalPruner * optPruner, const String & optOpTag)
{
   DECLARE_OP_TAG_GUARD;

   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->CloneDataNodeSubtree(sourceNode, destPath.StartsWith("/") ? destPath : DatabaseSubpathToSessionRelativePath(destPath, TreeGatewayFlags()), flags, optInsertBefore, optPruner) : B_BAD_OBJECT;
}

void MessageTreeDatabaseObject :: MessageReceivedFromTreeGatewaySubscriber(const ZGPeerID & fromPeerID, const MessageRef & payload, const String & tag)
{
   LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject #" UINT32_FORMAT_SPEC ":  MessageReceivedFromTreeGatewaySubscriber():  Unhandled Message (what=" UINT32_FORMAT_SPEC ", fromPeerID=[%s], tag=[%s])\n", GetDatabaseIndex(), payload()->what, fromPeerID.ToString()(), tag());
}

status_t MessageTreeDatabaseObject :: SendMessageToTreeGatewaySubscriber(const ZGPeerID & toPeerID, const String & tag, const MessageRef & payload)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   return zsh ? zsh->SendMessageToTreeGatewaySubscriber(toPeerID, tag, payload, GetDatabaseIndex()) : B_BAD_OBJECT;
}

SetDataNodeFlags MessageTreeDatabaseObject :: ConvertTreeGatewayFlagsToSetDataNodeFlags(TreeGatewayFlags tgf) const
{
   SetDataNodeFlags sdnf;
   if (tgf.IsBitSet(TREE_GATEWAY_FLAG_INDEXED))           sdnf.SetBit(SETDATANODE_FLAG_ADDTOINDEX);
#ifdef JAF_DELIBERATELY_REMOVED_THIS_LINE_BECAUSE_IT_PREVENTS_DATA_REPLICATION_FROM_WORKING_ACROSS_PEERS
   if (tgf.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY))           sdnf.SetBit(SETDATANODE_FLAG_QUIET);
#endif
   if (tgf.IsBitSet(TREE_GATEWAY_FLAG_DONTCREATENODE))    sdnf.SetBit(SETDATANODE_FLAG_DONTCREATENODE);
   if (tgf.IsBitSet(TREE_GATEWAY_FLAG_DONTOVERWRITEDATA)) sdnf.SetBit(SETDATANODE_FLAG_DONTOVERWRITEDATA);
   if (tgf.IsBitSet(TREE_GATEWAY_FLAG_ENABLESUPERCEDE))   sdnf.SetBit(SETDATANODE_FLAG_ENABLESUPERCEDE);
   return sdnf;
}

}; // end namespace zg

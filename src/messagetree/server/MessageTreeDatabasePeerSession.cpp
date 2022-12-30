#include "zg/messagetree/server/ClientDataMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"
#include "zg/messagetree/server/UndoStackMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"

namespace zg
{

enum {
   MTDPS_COMMAND_PINGSENIORPEER = 1836344432, // 'mtdp'
   MTDPS_COMMAND_MESSAGETOSENIORPEER,
   MTDPS_COMMAND_MESSAGEFROMSENIORPEER,
   MTDPS_COMMAND_MESSAGETOSUBSCRIBER,
};

static const String MTDPS_NAME_TAG     = "mtp_tag";
static const String MTDPS_NAME_SOURCE  = "mtp_src";
static const String MTDPS_NAME_FILTER  = "mtp_flt";
static const String MTDPS_NAME_FLAGS   = "mtp_flg";
static const String MTDPS_NAME_UNDOKEY = "mtp_key";
static const String MTDPS_NAME_PAYLOAD = "mtp_pay";
static const String MTDPS_NAME_WHICHDB = "mtp_dbi";
static const String MTDPS_NAME_PATH    = "mtp_pth";

MessageTreeDatabasePeerSession :: MessageTreeDatabasePeerSession(const ZGPeerSettings & zgPeerSettings) : ZGDatabasePeerSession(zgPeerSettings), ProxyTreeGateway(NULL), _muxGateway(this)
{
   SetRoutingFlag(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF, true);  // necessary because we want to be notified about updates to our own subtree
}

status_t MessageTreeDatabasePeerSession :: AttachedToServer()
{
   NestCountGuard ncd(_inPeerSessionSetupOrTeardown);
   status_t ret = ZGDatabasePeerSession::AttachedToServer();

   // Check for duplicate mount-points
   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++)
   {
      const MessageTreeDatabaseObject * idb = dynamic_cast<const MessageTreeDatabaseObject *>(GetDatabaseObject(i));
      for (uint32 j=0; j<numDBs; j++)
      {
         const MessageTreeDatabaseObject * jdb = dynamic_cast<const MessageTreeDatabaseObject *>(GetDatabaseObject(j));
         if ((idb)&&(jdb)&&(idb != jdb)&&(idb->GetRootPathWithoutSlash() == jdb->GetRootPathWithoutSlash()))
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeDatabasePeerSession::AttachedToServer:  Database #" UINT32_FORMAT_SPEC " has the same root-path [%s] as previously added database #" UINT32_FORMAT_SPEC "!\n", i, idb->GetRootPathWithoutSlash()(), j);
            return B_LOGIC_ERROR;
         }
      }
   }

   _gestaltMessage = GetEffectiveParameters();
   return ret;
}

void MessageTreeDatabasePeerSession :: AboutToDetachFromServer()
{
   NestCountGuard ncd(_inPeerSessionSetupOrTeardown);
   ZGDatabasePeerSession::AboutToDetachFromServer();
}

void MessageTreeDatabasePeerSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   const bool wasFullyAttached = IAmFullyAttached();
   ZGDatabasePeerSession::PeerHasComeOnline(peerID, peerInfo);
   if ((wasFullyAttached == false)&&(IAmFullyAttached()))
   {
      GatewayCallbackBatchGuard<ITreeGateway> gcbg(this);
      ProxyTreeGateway::TreeGatewayConnectionStateChanged();  // notify our subscribers that we're now connected to the database.
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   MessageRef cmdMsg;
   MRETURN_ON_ERROR(CreateMuscleSubscribeMessage(subscriptionPath, optFilterRef, flags, cmdMsg));

   NestCountGuard ncg(_inLocalRequestNestCount);
   MessageReceivedFromGateway(cmdMsg, NULL);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & /*optFilterRef*/, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   MRETURN_ON_ERROR(CreateMuscleUnsubscribeMessage(subscriptionPath, cmdMsg));
   MessageReceivedFromGateway(cmdMsg, NULL);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * /*calledBy*/, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   MRETURN_ON_ERROR(CreateMuscleUnsubscribeAllMessage(cmdMsg));
   MessageReceivedFromGateway(cmdMsg, NULL);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags /*flags*/, const String & tag)
{
   MessageRef cmdMsg;
   MRETURN_ON_ERROR(CreateMuscleRequestNodeValuesMessage(queryString, optFilterRef, cmdMsg, tag));

   NestCountGuard ncg(_inLocalRequestNestCount);
   MessageReceivedFromGateway(cmdMsg, NULL);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * /*calledBy*/, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   MRETURN_ON_ERROR(CreateMuscleRequestNodeSubtreesMessage(queryStrings, queryFilters, tag, maxDepth, cmdMsg));

   NestCountGuard ncg(_inLocalRequestNestCount);
   MessageReceivedFromGateway(cmdMsg, NULL);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstMessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->UploadNodeValue(relativePath, optPayload, flags, optBefore, optOpTag);
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::TreeGateway_UploadNodeValue():  No database found for path [%s]!\n", path());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * /*calledBy*/, const String & basePath, const ConstMessageRef & valuesMsg, TreeGatewayFlags flags, const String & optOpTag)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(basePath, &relativePath);
   if (mtDB)
   {
      // I'm not sure if this is the correct logic to use, but it works for now --jaf
      status_t ret;
      ConstMessageRef effMsg = valuesMsg;
      if (valuesMsg()->FindMessage(basePath, effMsg).IsError())
      {
         LogTime(MUSCLE_LOG_ERROR, "Couldn't find basePath Message [%s] in subtree-upload! [%s]\n", basePath(), ret());
         return ret;
      }

      (void) mtDB->RequestDeleteNodes(relativePath, ConstQueryFilterRef(), TreeGatewayFlags(), optOpTag);  // we want a full overwrite of the specified subtree, not an add-to
      return mtDB->UploadNodeSubtree(relativePath, effMsg, flags, optOpTag);
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::TreeGateway_UploadNodeSubtree():  No database found for path [%s]!\n", basePath());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   status_t ret;
   const Hashtable<MessageTreeDatabaseObject *, String> dbs = GetDatabasesForNodePath(path);
   for (HashtableIterator<MessageTreeDatabaseObject *, String> iter(dbs); iter.HasData(); iter++) ret |= iter.GetKey()->RequestDeleteNodes(iter.GetValue(), optFilterRef, flags, optOpTag);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const String & optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->RequestMoveIndexEntry(relativePath, optBefore, optFilterRef, flags, optOpTag);
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "TreeGateway_RequestMoveIndexEntry:  No database found for path [%s]\n", path());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, TreeGatewayFlags flags)
{
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY) == false) TreeLocalPeerPonged(tag);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, uint32 whichDB, TreeGatewayFlags flags)
{
   if (GetSeniorPeerID().IsValid() == false) return B_ERROR("PingSeniorPeer:  Senior peer not available");

   MessageRef seniorPingMsg = GetMessageFromPool(MTDPS_COMMAND_PINGSENIORPEER);
   MRETURN_OOM_ON_NULL(seniorPingMsg());

   MRETURN_ON_ERROR(seniorPingMsg()->CAddString(MTDPS_NAME_TAG,  tag));
   MRETURN_ON_ERROR(seniorPingMsg()->CAddFlat(MTDPS_NAME_SOURCE, GetLocalPeerID()));
   MRETURN_ON_ERROR(seniorPingMsg()->CAddFlat(MTDPS_NAME_FLAGS,  flags));
   return RequestUpdateDatabaseState(whichDB, seniorPingMsg);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * /*calledBy*/, const ConstMessageRef & msg, uint32 whichDB, const String & tag)
{
   if (GetSeniorPeerID().IsValid() == false) return B_ERROR("SendMessageToSeniorPeer:  Senior peer not available");

   MessageRef seniorCommandMsg = GetMessageFromPool(MTDPS_COMMAND_MESSAGETOSENIORPEER);
   MRETURN_OOM_ON_NULL(seniorCommandMsg());

   MRETURN_ON_ERROR(seniorCommandMsg()->AddMessage(MTDPS_NAME_PAYLOAD, CastAwayConstFromRef(msg)));
   MRETURN_ON_ERROR(seniorCommandMsg()->CAddFlat(  MTDPS_NAME_SOURCE,  GetLocalPeerID()));
   MRETURN_ON_ERROR(seniorCommandMsg()->CAddInt32( MTDPS_NAME_WHICHDB, whichDB));
   MRETURN_ON_ERROR(seniorCommandMsg()->CAddString(MTDPS_NAME_TAG,     tag));
   return SendUnicastUserMessageToPeer(GetSeniorPeerID(), seniorCommandMsg);
}

ZGPeerID MessageTreeDatabasePeerSession :: GetPerClientPeerIDForNode(const DataNode & node) const
{
   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++)
   {
      ClientDataMessageTreeDatabaseObject * nextDB = dynamic_cast<ClientDataMessageTreeDatabaseObject *>(GetDatabaseObject(i));
      if (nextDB)
      {
         String subPath;
         if (nextDB->GetDatabaseSubpath(node.GetNodePath(), &subPath) > 0)
         {
            ZGPeerID ret;
            ret.FromString(subPath());
            if (ret.IsValid()) return ret;
         }
      }
   }
   return ZGPeerID();
}

class GetPerClientPeerIDsCallbackArgs
{
public:
   GetPerClientPeerIDsCallbackArgs(Hashtable<ZGPeerID, Void> & peerIDs) : _peerIDs(peerIDs), _sendToAll(false) {/* empty */}

   Hashtable<ZGPeerID, Void> & _peerIDs;
   bool _sendToAll;
};

int MessageTreeDatabasePeerSession :: GetPerClientPeerIDsCallback(DataNode & node, void * ud)
{
   GetPerClientPeerIDsCallbackArgs & args = *(static_cast<GetPerClientPeerIDsCallbackArgs *>(ud));
   const ZGPeerID pid = GetPerClientPeerIDForNode(node);
   if (pid.IsValid())
   {
      (void) args._peerIDs.PutWithDefault(pid);
      return node.GetDepth();
   }
   else
   {
      args._sendToAll = true;
      return -1;  // abort now -- we know we'll need to send to all peers, so there's no sense traversing further
   }
}

static ZGPeerID GetPeerIDFromReturnAddress(const String & path, String * optRetSuffix)
{
   if (path.StartsWith('{'))
   {
      const int32 rightBraceIdx = path.IndexOf("}:");
      if (rightBraceIdx >= 0)
      {
         ZGPeerID ret;
         ret.FromString(path.Substring(1, rightBraceIdx));
         if (optRetSuffix) *optRetSuffix = path.Substring(rightBraceIdx+2);  // +1 for the right-brace, and +1 for the colon
         return ret;
      }
   }
   return ZGPeerID();  // failure
}


// Note:  If this method returns an error-code, that means we should send to all peers
status_t MessageTreeDatabasePeerSession :: GetPerClientPeerIDsForPath(const String & path, const ConstQueryFilterRef & optFilter, Hashtable<ZGPeerID, Void> & retPeerIDs)
{
   const ZGPeerID peerID = GetPeerIDFromReturnAddress(path, NULL);
   if (peerID.IsValid())
   {
      // path is something like "{f01898e8e4810001:fa4c50daa9eb}:_3_:_4_:" -- we'll use this to route it back to exactly one ITreeGatewaySubscriber
      return retPeerIDs.PutWithDefault(peerID);
   }
   else
   {
      // path is e.g. "foo/bar/baz*" -- we want to send the Message on to any ITreeGatewaySubscribers who are subscribed to any of the nodes matching the path
      const bool isGlobal = path.StartsWith('/');

      NodePathMatcher matcher;
      MRETURN_ON_ERROR(matcher.PutPathString(isGlobal?path.Substring(1):path, optFilter));

      GetPerClientPeerIDsCallbackArgs args(retPeerIDs);
      (void) matcher.DoTraversal((PathMatchCallback) GetPerClientPeerIDsCallbackFunc, this, isGlobal?GetGlobalRoot():*GetSessionNode()(), true, &args);
      return args._sendToAll ? B_ERROR : B_NO_ERROR;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriberPath, const ConstMessageRef & msg, const ConstQueryFilterRef & optFilterRef, const String & tag)
{
   MessageRef cmdMsg = GetMessageFromPool(MTDPS_COMMAND_MESSAGETOSUBSCRIBER);
   MRETURN_OOM_ON_NULL(cmdMsg());

   MRETURN_ON_ERROR(cmdMsg()->AddMessage(MTDPS_NAME_PAYLOAD,        CastAwayConstFromRef(msg)));
   MRETURN_ON_ERROR(cmdMsg()->CAddString(MTDPS_NAME_PATH,           subscriberPath));
   MRETURN_ON_ERROR(cmdMsg()->CAddArchiveMessage(MTDPS_NAME_FILTER, optFilterRef));
   MRETURN_ON_ERROR(cmdMsg()->AddString(MTDPS_NAME_TAG,             String("{%1}:%2").Arg(GetLocalPeerID().ToString()).Arg(tag)));

   Hashtable<ZGPeerID, Void> targetPeerIDs;
   if (GetPerClientPeerIDsForPath(subscriberPath, optFilterRef, targetPeerIDs).IsOK())
   {
      status_t ret;
      for (HashtableIterator<ZGPeerID, Void> iter(targetPeerIDs); iter.HasData(); iter++) ret |= SendUnicastUserMessageToPeer(iter.GetKey(), cmdMsg);
      return ret;
   }
   else return SendUnicastUserMessageToAllPeers(cmdMsg);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_BEGINSEQUENCE, optSequenceLabel, whichDB);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_EndUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_ENDSEQUENCE, optSequenceLabel, whichDB);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestUndo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB, const String & optOpTag)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_UNDO, optOpTag, whichDB);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestRedo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB, const String & optOpTag)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_REDO, optOpTag, whichDB);
}

status_t MessageTreeDatabasePeerSession :: UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabelOrOpTag, uint32 whichDB)
{
   UndoStackMessageTreeDatabaseObject * undoDB = dynamic_cast<UndoStackMessageTreeDatabaseObject *>(GetDatabaseObject(whichDB));
   if (undoDB)
   {
      return undoDB->UploadUndoRedoRequestToSeniorPeer(whatCode, optSequenceLabelOrOpTag);
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::UploadUndoRedoRequestToSeniorPeer():  Database #" UINT32_FORMAT_SPEC " is not an UndoStackMessageTreeDatabaseObject!\n", whichDB);
      return B_BAD_ARGUMENT;
   }
}

void MessageTreeDatabasePeerSession :: CommandBatchEnds()
{
   ProxyTreeGateway::CommandBatchEnds();
   if (IsAttachedToServer()) PushSubscriptionMessages();  // make sure any subscription updates go out in a timely fashion
}

void MessageTreeDatabasePeerSession :: NotifySubscribersThatNodeChanged(DataNode & node, const ConstMessageRef & oldDataRef, NodeChangeFlags nodeChangeFlags)
{
//printf("NotifySubscribersThatNodeChanged node=[%s] payload=%p nodeChangeFlags=%s\n", node.GetNodePath()(), node.GetData()(), nodeChangeFlags.ToHexString()());
   GatewayCallbackBatchGuard<ITreeGateway> gcbg(this);  // yes, this is necessary

   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), &relativePath);
   if (mtDB) mtDB->MessageTreeNodeUpdated(relativePath, node, oldDataRef, nodeChangeFlags.IsBitSet(NODE_CHANGE_FLAG_ISBEINGREMOVED));

   ZGDatabasePeerSession::NotifySubscribersThatNodeChanged(node, oldDataRef, nodeChangeFlags);
}

void MessageTreeDatabasePeerSession :: NotifySubscribersThatNodeIndexChanged(DataNode & node, char op, uint32 index, const String & key)
{
//printf("NotifySubscribersThatNodeIndexChanged node=[%s] op=%c index=%u key=[%s]\n", node.GetNodePath()(), op, index, key());
   GatewayCallbackBatchGuard<ITreeGateway> gcbg(this);  // yes, this is necessary

   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), &relativePath);
   if (mtDB) mtDB->MessageTreeNodeIndexChanged(relativePath, node, op, index, key);

   ZGDatabasePeerSession::NotifySubscribersThatNodeIndexChanged(node, op, index, key);
}

void MessageTreeDatabasePeerSession :: NodeChanged(DataNode & node, const ConstMessageRef & /*oldData*/, NodeChangeFlags nodeChangeFlags)
{
   // deliberately NOT calling up to superclass, as I don't want any MUSCLE-update messages to be generated for this session
   const MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), NULL);
   TreeNodeUpdated(node.GetNodePath().Substring(GetSessionRootPath().Length()+1), nodeChangeFlags.IsBitSet(NODE_CHANGE_FLAG_ISBEINGREMOVED)?ConstMessageRef():node.GetData(), mtDB?mtDB->GetCurrentOpTag():GetEmptyString());
}

void MessageTreeDatabasePeerSession :: NodeIndexChanged(DataNode & node, char op, uint32 index, const String & key)
{
   // deliberately NOT calling up to superclass, as I don't want any MUSCLE-update messages to be generated for this session
   const MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), NULL);
   const String & opTag = mtDB?mtDB->GetCurrentOpTag():GetEmptyString();
   const String path = node.GetNodePath().Substring(GetSessionRootPath().Length()+1);
   switch(op)
   {
      case INDEX_OP_ENTRYINSERTED: TreeNodeIndexEntryInserted(path, index, key, opTag); break;
      case INDEX_OP_ENTRYREMOVED:  TreeNodeIndexEntryRemoved( path, index, key, opTag); break;
      case INDEX_OP_CLEARED:       TreeNodeIndexCleared(path, opTag);                   break;
      default:                     LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTreeDatabasePeerSession::NodeIndexChanged:  Unknown op [%c] for node [%s]\n", op, path());
   }
}

MessageTreeDatabaseObject * MessageTreeDatabasePeerSession :: GetDatabaseForNodePath(const String & nodePath, String * optRetRelativePath) const
{
   uint32 closestDist = MUSCLE_NO_LIMIT;
   MessageTreeDatabaseObject * ret = NULL;
   String closestSubpath, temp;

   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++)
   {
      MessageTreeDatabaseObject * nextDB = dynamic_cast<MessageTreeDatabaseObject *>(GetDatabaseObject(i));
      if (nextDB)
      {
         const int32 dist = nextDB->GetDatabaseSubpath(nodePath, &temp);
         if ((dist >= 0)&&(((uint32)dist) < closestDist))
         {
            ret            = nextDB;
            closestDist    = dist;
            closestSubpath = temp;
         }
      }
   }

   if (optRetRelativePath) *optRetRelativePath = closestSubpath;
   return ret;
}

Hashtable<MessageTreeDatabaseObject *, String> MessageTreeDatabasePeerSession :: GetDatabasesForNodePath(const String & nodePath) const
{
   Hashtable<MessageTreeDatabaseObject *, String> ret;

   String temp;

   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++)
   {
      MessageTreeDatabaseObject * nextDB = dynamic_cast<MessageTreeDatabaseObject *>(GetDatabaseObject(i));
      if ((nextDB)&&(nextDB->GetDatabaseSubpath(nodePath, &temp) >= 0)) (void) ret.Put(nextDB, temp);
   }
   return ret;
}

ConstMessageRef MessageTreeDatabasePeerSession :: SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg)
{
   switch(seniorDoMsg()->what)
   {
      case MTDPS_COMMAND_PINGSENIORPEER:
         HandleSeniorPeerPingMessage(whichDatabase, seniorDoMsg);
         return seniorDoMsg;
      break;

      default:
         return ZGDatabasePeerSession::SeniorUpdateLocalDatabase(whichDatabase, dbChecksum, seniorDoMsg);
   }
}

status_t MessageTreeDatabasePeerSession :: JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg)
{
   switch(juniorDoMsg()->what)
   {
      case MTDPS_COMMAND_PINGSENIORPEER:
         HandleSeniorPeerPingMessage(whichDatabase, juniorDoMsg);
      return B_NO_ERROR;

      default:
         return ZGDatabasePeerSession::JuniorUpdateLocalDatabase(whichDatabase, dbChecksum, juniorDoMsg);
   }
}

void MessageTreeDatabasePeerSession :: HandleSeniorPeerPingMessage(uint32 whichDatabase, const ConstMessageRef & msg)
{
   const ZGPeerID sourcePeerID = msg()->GetFlat<ZGPeerID>(MTDPS_NAME_SOURCE);
   TreeGatewayFlags flags      = msg()->GetFlat<TreeGatewayFlags>(MTDPS_NAME_FLAGS);
   if ((flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY) == false)&&(sourcePeerID == GetLocalPeerID())) TreeSeniorPeerPonged(msg()->GetStringReference(MTDPS_NAME_TAG), whichDatabase);
}

status_t MessageTreeDatabasePeerSession :: GetUnusedNodeID(const String & path, uint32 & retID)
{
   const DataNode * node = GetDataNode(path);
   if (node == NULL)
   {
      // If there is no parent node currently, demand-create it
      MessageRef emptyRef(GetMessageFromPool());
      MRETURN_OOM_ON_NULL(emptyRef());
      MRETURN_ON_ERROR(SetDataNode(path, emptyRef));
      return GetUnusedNodeID(path, retID);
   }

   const uint32 NUM_NODE_IDS = 100000;  // chosen arbitrarily
   uint32 nextID = (node->GetMaxKnownChildIDHint()%NUM_NODE_IDS);
   for (uint32 i=0; i<NUM_NODE_IDS; i++)
   {
      char temp[32];
      muscleSprintf(temp, "I" UINT32_FORMAT_SPEC, nextID);
      if ((node->HasChild(&temp[1]))||(node->HasChild(temp))) nextID = (nextID+1)%NUM_NODE_IDS;
      else
      {
         retID = nextID;
         return B_NO_ERROR;
      }
   }

   LogTime(MUSCLE_LOG_CRITICALERROR, "GetUnusedNodeID():  Could not find available child ID for node path [%s]!\n", path());
   return B_ERROR("Node IDs exhausted");
}

ServerSideMessageTreeSession * MessageTreeDatabasePeerSession :: GetActiveServerSideMessageTreeSession() const
{
   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      ServerSideMessageTreeSession * ssmts = dynamic_cast<ServerSideMessageTreeSession *>(iter.GetValue()());
      if ((ssmts)&&(ssmts->IsInMessageReceivedFromGateway())) return ssmts;
   }
   return NULL;
}

void MessageTreeDatabasePeerSession :: ServerSideMessageTreeSessionIsDetaching(ServerSideMessageTreeSession * clientSession)
{
   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++)
   {
      ClientDataMessageTreeDatabaseObject * clientDB = dynamic_cast<ClientDataMessageTreeDatabaseObject *>(GetDatabaseObject(i));
      if (clientDB) clientDB->ServerSideMessageTreeSessionIsDetaching(clientSession);
   }
}

const String & MessageTreeDatabasePeerSession :: GetCurrentOpTagForNodePath(const String & nodePath) const
{
   const MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(nodePath, NULL);
   return mtDB ? mtDB->GetCurrentOpTag() : GetEmptyString();
}

int MessageTreeDatabasePeerSession :: GetSubscribedSessionsCallback(DataNode & node, void * ud)
{
   Hashtable<ServerSideMessageTreeSession *, Void> & results = *(static_cast<Hashtable<ServerSideMessageTreeSession *, Void> *>(ud));
   for (HashtableIterator<uint32, uint32> iter(node.GetSubscribers()); iter.HasData(); iter++)
   {
      ServerSideMessageTreeSession * ssmts = dynamic_cast<ServerSideMessageTreeSession *>(GetSession(iter.GetKey())());
      if (ssmts) (void) results.PutWithDefault(ssmts);
   }
   return node.GetDepth();
}

void MessageTreeDatabasePeerSession :: MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg)
{
   switch(msg()->what)
   {
      case MTDPS_COMMAND_MESSAGETOSENIORPEER:
         if (IAmTheSeniorPeer())
         {
            MessageRef payload          = msg()->GetMessage(MTDPS_NAME_PAYLOAD);
            const ZGPeerID sourcePeerID = msg()->GetFlat<ZGPeerID>(MTDPS_NAME_SOURCE);
            const uint32 whichDB        = msg()->GetInt32(MTDPS_NAME_WHICHDB);
            const String & tag          = msg()->GetStringReference(MTDPS_NAME_TAG);
            if (payload())
            {
               MessageReceivedFromTreeGatewaySubscriber(sourcePeerID, payload, whichDB, tag);
            }
            else LogTime(MUSCLE_LOG_ERROR, "Peer [%s] Received MTDPS_COMMAND_MESSAGETOSENIORPEER, but it has no payload!\n", GetLocalPeerID().ToString()());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Peer [%s] Received MTDPS_COMMAND_MESSAGETOSENIORPEER, but I am not the senior peer!\n", GetLocalPeerID().ToString()());
      break;

      case MTDPS_COMMAND_MESSAGEFROMSENIORPEER:
      {
         MessageRef payload            = msg()->GetMessage(MTDPS_NAME_PAYLOAD);
         //const ZGPeerID sourcePeerID = msg()->GetFlat<ZGPeerID>(MTDPS_NAME_SOURCE);
         const uint32 whichDB          = msg()->GetInt32(MTDPS_NAME_WHICHDB);
         const String & tag            = msg()->GetStringReference(MTDPS_NAME_TAG);
         if (payload())
         {
            MessageReceivedFromTreeSeniorPeer(whichDB, tag, payload);
         }
         else LogTime(MUSCLE_LOG_ERROR, "Peer [%s] Received MTDPS_COMMAND_MESSAGETOSENIORPEER, but it has no payload!\n", GetLocalPeerID().ToString()());
      }
      break;

      case MTDPS_COMMAND_MESSAGETOSUBSCRIBER:
      {
         QueryFilterRef qfRef;
         {
            MessageRef qfMsg = msg()->GetMessage(MTDPS_NAME_FILTER);
            if (qfMsg()) qfRef = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg());
         }

         MessageRef payload  = msg()->GetMessage(MTDPS_NAME_PAYLOAD);
         const String & path = msg()->GetStringReference(MTDPS_NAME_PATH);
         const String & tag  = msg()->GetStringReference(MTDPS_NAME_TAG);
         if (payload())
         {
            String suffix;  // will contain everything but the {peerID}:
            const ZGPeerID targetPeerID = GetPeerIDFromReturnAddress(path, &suffix);
            if (targetPeerID.IsValid())
            {
               if (targetPeerID == GetLocalPeerID())
               {
                  MessageReceivedFromSubscriber(suffix, payload, tag);
               }
               else LogTime(MUSCLE_LOG_ERROR, "Peer [%s] Received MTDPS_COMMAND_MESSAGETOSUBSCRIBER addressed to peer [%s]!\n", GetLocalPeerID().ToString()(), targetPeerID.ToString()());
            }
            else
            {
               // We want to forward this Message on to any local clients that are subscribed to any nodes matched by (path)
               const bool isGlobal = path.StartsWith('/');

               NodePathMatcher matcher;
               (void) matcher.PutPathString(isGlobal?path.Substring(1):path, qfRef);

               Hashtable<ServerSideMessageTreeSession *, Void> subscribedSessions;
               (void) matcher.DoTraversal((PathMatchCallback) GetSubscribedSessionsCallbackFunc, this, isGlobal?GetGlobalRoot():*GetSessionNode()(), true, &subscribedSessions);
               for (HashtableIterator<ServerSideMessageTreeSession *, Void> iter(subscribedSessions); iter.HasData(); iter++) iter.GetKey()->MessageReceivedFromSubscriber(path, payload, tag);
            }
         }
         else LogTime(MUSCLE_LOG_ERROR, "Peer [%s] Received MTDPS_COMMAND_MESSAGETOSUBSCRIBER, but it has no payload!\n", GetLocalPeerID().ToString()());
      }
      break;

      default:
         ZGDatabasePeerSession::MessageReceivedFromPeer(fromPeerID, msg);
      break;
   }
}

void MessageTreeDatabasePeerSession :: MessageReceivedFromTreeGatewaySubscriber(const ZGPeerID & fromPeerID, const MessageRef & payload, uint32 whichDB, const String & tag)
{
   // Default implementation will try to pass the Message on to one of our Database objects
   MessageTreeDatabaseObject * db = dynamic_cast<MessageTreeDatabaseObject *>(GetDatabaseObject(whichDB));
   if (db) db->MessageReceivedFromTreeGatewaySubscriber(fromPeerID, payload, tag);
      else LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::MessageReceivedFromTreeGatewaySubscriber:  Database #" UINT32_FORMAT_SPEC " is not a MessageTreeDatabaseObject!\n", whichDB);
}

status_t MessageTreeDatabasePeerSession :: SendMessageToTreeGatewaySubscriber(const ZGPeerID & toPeerID, const String & tag, const ConstMessageRef & payload, int32 optWhichDB)
{
   MessageRef replyMsg = GetMessageFromPool(MTDPS_COMMAND_MESSAGEFROMSENIORPEER);
   MRETURN_OOM_ON_NULL(replyMsg());

   const status_t ret = replyMsg()->AddMessage(MTDPS_NAME_PAYLOAD, CastAwayConstFromRef(payload))
                      | replyMsg()->CAddFlat(  MTDPS_NAME_SOURCE,  GetLocalPeerID())
                      | replyMsg()->CAddInt32( MTDPS_NAME_WHICHDB, optWhichDB)
                      | replyMsg()->CAddString(MTDPS_NAME_TAG,     tag);
   return ret.IsOK() ? SendUnicastUserMessageToPeer(toPeerID, replyMsg) : ret;
}

ConstMessageRef MessageTreeDatabasePeerSession :: TreeGateway_GetGestaltMessage() const
{
   return _gestaltMessage;
}


status_t MessageTreeDatabasePeerSession :: ConvertPathToSessionRelative(String & path) const
{
   if (GetPathDepth(path()) >= NODE_DEPTH_USER)
   {
      path = GetPathClause(NODE_DEPTH_USER, path());
      return B_NO_ERROR;
   }
   return B_BAD_ARGUMENT;
}

void MessageTreeDatabasePeerSession :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData)
{
   if ((&from == this)&&(_inLocalRequestNestCount.IsInBatch()))
   {
      // Gotta handle some replies locally as they need to go back to our MuxTreeGateway for local distribution
      bool handled = true;
      switch(msg()->what)
      {
         case PR_RESULT_DATATREES:
         {
            String opTag; if (msg()->FindString(PR_NAME_TREE_REQUEST_ID, opTag).IsOK()) (void) msg()->RemoveName(PR_NAME_TREE_REQUEST_ID);
            _muxGateway.SubtreesRequestResultReturned(opTag, msg);
         }
         break;

         case PR_RESULT_DATAITEMS:
         {
            // Handle notifications of removed nodes
            {
               String nodePath;
               for (int i=0; msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, nodePath).IsOK(); i++)
                  if (ConvertPathToSessionRelative(nodePath).IsOK())
                     _muxGateway.TreeNodeUpdated(nodePath, ConstMessageRef(), GetEmptyString());
            }

            // Handle notifications of added/updated nodes
            {
               uint32 currentFieldNameIndex = 0;
               MessageRef nodeRef;
               for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(); iter.HasData(); iter++,currentFieldNameIndex++)
               {
                  String nodePath = iter.GetFieldName();
                  if (ConvertPathToSessionRelative(nodePath).IsOK())
                     for (uint32 i=0; msg()->FindMessage(iter.GetFieldName(), i, nodeRef).IsOK(); i++)
                        _muxGateway.TreeNodeUpdated(nodePath, nodeRef, GetEmptyString());
               }
            }
         }
         break;

         case PR_RESULT_INDEXUPDATED:
         {
            // Handle notifications of node-index changes
            uint32 currentFieldNameIndex = 0;
            for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(); iter.HasData(); iter++,currentFieldNameIndex++)
            {
               String sessionRelativePath = iter.GetFieldName();
               if (ConvertPathToSessionRelative(sessionRelativePath).IsOK())
               {
                  const char * indexCmd;
                  for (int i=0; msg()->FindString(iter.GetFieldName(), i, &indexCmd).IsOK(); i++)
                  {
                     const char * colonAt = strchr(indexCmd, ':');
                     char c = indexCmd[0];
                     switch(c)
                     {
                        case INDEX_OP_CLEARED:       _muxGateway.TreeNodeIndexCleared(      sessionRelativePath, GetEmptyString());                                             break;
                        case INDEX_OP_ENTRYINSERTED: _muxGateway.TreeNodeIndexEntryInserted(sessionRelativePath, atoi(&indexCmd[1]), colonAt?(colonAt+1):"", GetEmptyString()); break;
                        case INDEX_OP_ENTRYREMOVED:  _muxGateway.TreeNodeIndexEntryRemoved( sessionRelativePath, atoi(&indexCmd[1]), colonAt?(colonAt+1):"", GetEmptyString()); break;
                     }
                  }
               }
            }
         }
         break;

         default:
            handled = false;
         break;
      }

      if (handled) return;
   }

   ZGDatabasePeerSession::MessageReceivedFromSession(from, msg, userData);
}

};  // end namespace zg

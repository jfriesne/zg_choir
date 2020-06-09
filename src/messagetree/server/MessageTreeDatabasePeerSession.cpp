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
};

static const String MTDPS_NAME_TAG     = "mtp_tag";
static const String MTDPS_NAME_SOURCE  = "mtp_src";
static const String MTDPS_NAME_FLAGS   = "mtp_flg";
static const String MTDPS_NAME_UNDOKEY = "mtp_key";

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
   const status_t ret = CreateMuscleSubscribeMessage(subscriptionPath, optFilterRef, flags, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & /*optFilterRef*/, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleUnsubscribeMessage(subscriptionPath, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * /*calledBy*/, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleUnsubscribeAllMessage(cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleRequestNodeValuesMessage(queryString, optFilterRef, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * /*calledBy*/, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleRequestNodeSubtreesMessage(queryStrings, queryFilters, tag, maxDepth, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->UploadNodeValue(relativePath, optPayload, flags, optBefore);
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::TreeGateway_UploadNodeValue():  No database found for path [%s]!\n", path());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * /*calledBy*/, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(basePath, &relativePath);
   if (mtDB)
   {
      // I'm not sure if this is the correct logic to use, but it works for now --jaf
      status_t ret;
      MessageRef effMsg = valuesMsg;
      if (valuesMsg()->FindMessage(basePath, effMsg).IsError())
      {
         LogTime(MUSCLE_LOG_ERROR, "Couldn't find basePath Message [%s] in subtree-upload! [%s]\n", basePath(), ret());
         return ret;
      }

      (void) mtDB->RequestDeleteNodes(relativePath, ConstQueryFilterRef(), TreeGatewayFlags());  // we want a full overwrite of the specified subtree, not an add-to
      return mtDB->UploadNodeSubtree(relativePath, effMsg, flags);
   }
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::TreeGateway_UploadNodeSubtree():  No database found for path [%s]!\n", basePath());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   status_t ret;
   const Hashtable<MessageTreeDatabaseObject *, String> dbs = GetDatabasesForNodePath(path);
   for (HashtableIterator<MessageTreeDatabaseObject *, String> iter(dbs); iter.HasData(); iter++) ret |= iter.GetKey()->RequestDeleteNodes(iter.GetValue(), optFilterRef, flags);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const String * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->RequestMoveIndexEntry(relativePath, optBefore, optFilterRef, flags); 
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
   if (seniorPingMsg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret = seniorPingMsg()->CAddString(MTDPS_NAME_TAG,  tag)
                | seniorPingMsg()->CAddFlat(MTDPS_NAME_SOURCE, GetLocalPeerID())
                | seniorPingMsg()->CAddFlat(MTDPS_NAME_FLAGS,  flags);
   return ret.IsOK() ? RequestUpdateDatabaseState(whichDB, seniorPingMsg) : ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_BEGINSEQUENCE, optSequenceLabel, whichDB);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_EndUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_ENDSEQUENCE, optSequenceLabel, whichDB);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestUndo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_UNDO, GetEmptyString(), whichDB);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestRedo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB)
{
   return UploadUndoRedoRequestToSeniorPeer(UNDOSTACK_COMMAND_REDO, GetEmptyString(), whichDB);
}

status_t MessageTreeDatabasePeerSession :: UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabel, uint32 whichDB)
{
   UndoStackMessageTreeDatabaseObject * undoDB = dynamic_cast<UndoStackMessageTreeDatabaseObject *>(GetDatabaseObject(whichDB));
   if (undoDB)
   {
      return undoDB->UploadUndoRedoRequestToSeniorPeer(whatCode, optSequenceLabel);
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

void MessageTreeDatabasePeerSession :: NotifySubscribersThatNodeChanged(DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved)
{
//printf("NotifySubscribersThatNodeChanged node=[%s] payload=%p isBeingRemoved=%i\n", node.GetNodePath()(), node.GetData()(), isBeingRemoved);
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), &relativePath);
   if (mtDB) mtDB->MessageTreeNodeUpdated(relativePath, node, oldDataRef, isBeingRemoved);

   ZGDatabasePeerSession::NotifySubscribersThatNodeChanged(node, oldDataRef, isBeingRemoved);
}

void MessageTreeDatabasePeerSession :: NotifySubscribersThatNodeIndexChanged(DataNode & node, char op, uint32 index, const String & key)
{
//printf("NotifySubscribersThatNodeIndexChanged node=[%s] op=%c index=%u key=[%s]\n", node.GetNodePath()(), op, index, key());
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), &relativePath);
   if (mtDB) mtDB->MessageTreeNodeIndexChanged(relativePath, node, op, index, key);

   ZGDatabasePeerSession::NotifySubscribersThatNodeIndexChanged(node, op, index, key);
}

void MessageTreeDatabasePeerSession :: NodeChanged(DataNode & node, const MessageRef & /*oldData*/, bool isBeingRemoved)
{
   // deliberately NOT calling up to superclass, as I don't want any MUSCLE-update messages to be generated for this session
   TreeNodeUpdated(node.GetNodePath().Substring(GetSessionRootPath().Length()), isBeingRemoved?MessageRef():node.GetData());
}

void MessageTreeDatabasePeerSession :: NodeIndexChanged(DataNode & node, char op, uint32 index, const String & key)
{
   // deliberately NOT calling up to superclass, as I don't want any MUSCLE-update messages to be generated for this session
   const String path = node.GetNodePath().Substring(GetSessionRootPath().Length());
   switch(op)
   {
      case INDEX_OP_ENTRYINSERTED: TreeNodeIndexEntryInserted(path, index, key); break;
      case INDEX_OP_ENTRYREMOVED:  TreeNodeIndexEntryRemoved( path, index, key); break;
      case INDEX_OP_CLEARED:       TreeNodeIndexCleared(path);                   break;
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
   const String * tag          = msg()->GetStringPointer(MTDPS_NAME_TAG);
   const ZGPeerID sourcePeerID = msg()->GetFlat<ZGPeerID>(MTDPS_NAME_SOURCE);
   TreeGatewayFlags flags      = msg()->GetFlat<TreeGatewayFlags>(MTDPS_NAME_FLAGS);
   if ((flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY) == false)&&(sourcePeerID == GetLocalPeerID())) TreeSeniorPeerPonged(*tag, whichDatabase);
}

status_t MessageTreeDatabasePeerSession :: GetUnusedNodeID(const String & path, uint32 & retID)
{
   const DataNode * node = GetDataNode(path);
   if (node == NULL)
   {
      // If there is no parent node currently, demand-create it
      MessageRef emptyRef(GetMessageFromPool());
      return ((emptyRef())&&(SetDataNode(path, emptyRef) == B_NO_ERROR)) ? GetUnusedNodeID(path, retID) : B_ERROR;
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

};  // end namespace zg

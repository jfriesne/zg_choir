#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"

namespace zg
{

enum {
   MTDPS_COMMAND_PINGSENIORPEER = 1836344432, // 'mtdp' 
};

static const String MTDPS_NAME_TAG    = "mtp_tag";
static const String MTDPS_NAME_SOURCE = "mtp_src";
static const String MTDPS_NAME_FLAGS  = "mtp_flg";

MessageTreeDatabasePeerSession :: MessageTreeDatabasePeerSession(const ZGPeerSettings & zgPeerSettings) : ZGDatabasePeerSession(zgPeerSettings), ProxyTreeGateway(NULL), _muxGateway(this)
{
   SetRoutingFlag(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF, true);  // necessary because we want to be notified about updates to our own subtree
}

void MessageTreeDatabasePeerSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   const bool wasFullyAttached = IAmFullyAttached();
   ZGDatabasePeerSession::PeerHasComeOnline(peerID, peerInfo);
   if ((wasFullyAttached == false)&&(IAmFullyAttached())) ProxyTreeGateway::TreeGatewayConnectionStateChanged();  // notify our subscribers that we're now connected to the database.
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG AddSubscription [%s] qf=%p\n", subscriptionPath(), optFilterRef());
   return AddRemoveSubscriptionAux(PR_COMMAND_SETPARAMETERS, subscriptionPath, optFilterRef, flags);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG RemoveSubscription [%s]\n", subscriptionPath());
   return AddRemoveSubscriptionAux(PR_COMMAND_REMOVEPARAMETERS, subscriptionPath, optFilterRef, flags);
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags)
{
printf("ZG RemoveAllSubscriptions\n");
   MessageRef msg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if (msg()->AddBool("SUBSCRIBE:*", true).IsOK(ret)) MessageReceivedFromGateway(msg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: AddRemoveSubscriptionAux(uint32 whatCode, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   const String pathArg = ((whatCode == PR_COMMAND_SETPARAMETERS) ? subscriptionPath : EscapeRegexTokens(subscriptionPath)).Prepend("SUBSCRIBE:");  // don't accidentally remove multiple subscriptions due to wildcarding!
   const status_t ret   = (optFilterRef() ? msg()->CAddArchiveMessage(pathArg, optFilterRef) : msg()->AddBool(pathArg, true))
                        | msg()->CAddBool(PR_NAME_SUBSCRIBE_QUIETLY, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
   if (ret.IsOK()) MessageReceivedFromGateway(msg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG RequestNodeValues [%s]\n", queryString());

   MessageRef msg = GetMessageFromPool(PR_COMMAND_GETDATA);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if ((msg()->AddString(PR_NAME_KEYS, queryString).IsOK(ret)) && (msg()->CAddArchiveMessage(PR_NAME_FILTERS, optFilterRef).IsOK(ret))) MessageReceivedFromGateway(msg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
printf("ZG RequestNodeSubtrees [%s]\n", queryStrings.HeadWithDefault()());
   MessageRef msg = GetMessageFromPool(PR_COMMAND_GETDATATREES);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   uint32 numQueryStrings = queryStrings.GetNumItems();
   for (uint32 i=0; i<numQueryStrings; i++)
   {
      if (msg()->AddString(PR_NAME_KEYS, queryStrings[i]).IsError(ret)) return ret;
      if ((i<queryFilters.GetNumItems())&&(msg()->CAddArchiveMessage(PR_NAME_FILTERS, queryFilters[i]).IsError(ret))) return ret;
   }
   if (msg()->CAddInt32(PR_NAME_MAXDEPTH, maxDepth, MUSCLE_NO_LIMIT).IsError(ret)) return ret;
   if (msg()->AddString(PR_NAME_TREE_REQUEST_ID, tag).IsError(ret)) return ret;

   MessageReceivedFromGateway(msg, NULL);
   return ret;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
printf("ZG UploadNodeValue [%s] %p\n", path(), optPayload());
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->UploadNodeValue(relativePath, optPayload, flags, optBefore);
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabasePeerSession::TreeGateway_UploadNodeValue():  No database found for path [%s]!\n", path());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
printf("ZG UploadNodeSubtree [%s]\n", basePath());
return B_UNIMPLEMENTED;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG RequestDeleteNodes [%s]\n", path());
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->RequestDeleteNodes(relativePath, optFilterRef, flags); 
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "TreeGateway_RequestDeleteNodes:  No database found for path [%s]\n", path());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("ZG RequestMoveIndexEntry [%s]\n", path());
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(path, &relativePath);
   if (mtDB) return mtDB->RequestMoveIndexEntry(relativePath, optBefore, optFilterRef, flags); 
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "TreeGateway_RequestMoveIndexEntry:  No database found for path [%s]\n", path());
      return B_BAD_ARGUMENT;
   }
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags)
{
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY) == false) TreeServerPonged(tag);
   return B_NO_ERROR;
}

status_t MessageTreeDatabasePeerSession :: TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, uint32 whichDB, const String & tag, TreeGatewayFlags flags)
{
printf("ZG PingSeniorPeer [%s]\n", tag());
   if (GetSeniorPeerID().IsValid() == false) return B_ERROR("PingSeniorPeer:  Senior peer not available");

   MessageRef seniorPingMsg = GetMessageFromPool(MTDPS_COMMAND_PINGSENIORPEER);
   if (seniorPingMsg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret = seniorPingMsg()->CAddString(MTDPS_NAME_TAG,  tag)
                | seniorPingMsg()->CAddFlat(MTDPS_NAME_SOURCE, GetLocalPeerID())
                | seniorPingMsg()->CAddFlat(MTDPS_NAME_FLAGS,  flags);
   return ret.IsOK() ? RequestUpdateDatabaseState(whichDB, seniorPingMsg) : ret;
}

void MessageTreeDatabasePeerSession :: CommandBatchEnds()
{
   ProxyTreeGateway::CommandBatchEnds();
   if (IsAttachedToServer()) PushSubscriptionMessages();  // make sure any subscription updates go out in a timely fashion
}

void MessageTreeDatabasePeerSession :: NotifySubscribersThatNodeChanged(DataNode & node, const MessageRef & oldDataRef, bool isBeingRemoved)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), &relativePath);
   if (mtDB) mtDB->MessageTreeNodeUpdated(relativePath, node, oldDataRef, isBeingRemoved);

   ZGDatabasePeerSession::NotifySubscribersThatNodeChanged(node, oldDataRef, isBeingRemoved);
}

void MessageTreeDatabasePeerSession :: NotifySubscribersThatNodeIndexChanged(DataNode & node, char op, uint32 index, const String & key)
{
   String relativePath;
   MessageTreeDatabaseObject * mtDB = GetDatabaseForNodePath(node.GetNodePath(), &relativePath);
   if (mtDB) mtDB->MessageTreeNodeIndexChanged(relativePath, node, op, index, key);

   ZGDatabasePeerSession::NotifySubscribersThatNodeIndexChanged(node, op, index, key);
}

void MessageTreeDatabasePeerSession :: NodeChanged(DataNode & node, const MessageRef & oldData, bool isBeingRemoved)
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

MessageTreeDatabaseObject * MessageTreeDatabasePeerSession :: GetDatabaseForNodePath(const String & nodePath, String * optRetRelativePath)
{
   uint32 closestDist = MUSCLE_NO_LIMIT;
   MessageTreeDatabaseObject * ret = NULL;

   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++)
   {
      MessageTreeDatabaseObject * nextDB = dynamic_cast<MessageTreeDatabaseObject *>(GetDatabaseObject(i));
      if (nextDB)
      {
         const int32 dist = nextDB->GetDistanceFromDatabaseRootToNode(nodePath);
         if ((dist >= 0)&&(((uint32)dist) < closestDist))
         {
            ret         = nextDB;
            closestDist = dist;
         }
      }
   }

   return ((ret)&&(ret->GetDatabaseSubpath(nodePath, optRetRelativePath).IsOK())) ? ret : NULL;
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
      break;

      default:
         return ZGDatabasePeerSession::JuniorUpdateLocalDatabase(whichDatabase, dbChecksum, juniorDoMsg);
   }
}

void MessageTreeDatabasePeerSession :: HandleSeniorPeerPingMessage(uint32 whichDatabase, const ConstMessageRef & msg)
{
   const String * tag          = msg()->GetStringPointer(MTDPS_NAME_TAG);
   const ZGPeerID sourcePeerID = msg()->GetFlat<ZGPeerID>(MTDPS_NAME_SOURCE);
   TreeGatewayFlags flags      = msg()->GetFlat<TreeGatewayFlags>(MTDPS_NAME_FLAGS);
   if ((flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY) == false)&&(sourcePeerID == GetLocalPeerID())) TreeSeniorPeerPonged(whichDatabase, *tag);
}
};  // end namespace zg

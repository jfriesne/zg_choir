#include "zg/gateway/tree/NetworkTreeGateway.h"
#include "regex/QueryFilter.h"          // for CreateQueryFilter()
#include "util/MiscUtilityFunctions.h"  // for AssembleBatchMesssage()

namespace zg {

// Command-codes for Messages sent from client to server
enum {
   NTG_COMMAND_ADDSUBSCRIPTION = 1852269360,  // 'ngc0' 
   NTG_COMMAND_REMOVESUBSCRIPTION,
   NTG_COMMAND_REMOVEALLSUBSCRIPTIONS,
   NTG_COMMAND_REQUESTNODESUBTREES,
   NTG_COMMAND_REQUESTNODEVALUES,
   NTG_COMMAND_PING,
   NTG_COMMAND_UPLOADNODEVALUE,
   NTG_COMMAND_UPLOADNODESUBTREE,
   NTG_COMMAND_REMOVENODES,
   NTG_COMMAND_REORDERNODES,
};

// Reply-codes for Messages sent from server to client
enum {
   NTG_REPLY_NODEUPDATED = 1852273200,  // 'ngr0' 
   NTG_REPLY_INDEXCLEARED,
   NTG_REPLY_INDEXENTRYINSERTED,
   NTG_REPLY_INDEXENTRYREMOVED,
   NTG_REPLY_PONG,
   NTG_REPLY_SUBTREES,
};

static const String NTG_NAME_PATH        = "ntg_pth";
static const String NTG_NAME_QUERYFILTER = "ntg_qf";
static const String NTG_NAME_PAYLOAD     = "ntg_pay";
static const String NTG_NAME_FLAGS       = "ntg_flg";
static const String NTG_NAME_TAG         = "ntg_tag";
static const String NTG_NAME_MAXDEPTH    = "ntg_max";
static const String NTG_NAME_BEFORE      = "ntg_b4";
static const String NTG_NAME_INDEX       = "ntg_idx";
static const String NTG_NAME_NAME        = "ntg_nam";

NetworkTreeGateway :: NetworkTreeGateway(ITreeGateway * optUpstreamGateway, INetworkMessageSender * messageSender)
   : ProxyTreeGateway(optUpstreamGateway)
   , _messageSender(messageSender)
   , _isConnected(false)
{
   // empty
}

NetworkTreeGateway :: ~NetworkTreeGateway()
{
   // empty
}

status_t NetworkTreeGateway :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("AddSubscription [%s]\n", subscriptionPath());
   return HandleBasicCommandAux(NTG_COMMAND_ADDSUBSCRIPTION, subscriptionPath, optFilterRef, flags);
}

status_t NetworkTreeGateway :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("RemoveSubscription [%s]\n", subscriptionPath());
   return HandleBasicCommandAux(NTG_COMMAND_REMOVESUBSCRIPTION, subscriptionPath, optFilterRef, flags);
}

status_t NetworkTreeGateway :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * /*calledBy*/, TreeGatewayFlags flags)
{
printf("RemoveAllSubscriptions\n");
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_REMOVEALLSUBSCRIPTIONS);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   return msg()->CAddFlat(NTG_NAME_FLAGS, flags).IsOK(ret) ? AddOrSendOutgoingMessage(msg) : ret;
}

status_t NetworkTreeGateway :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("RequestNodeValues [%s]\n", queryString());
   return HandleBasicCommandAux(NTG_COMMAND_REQUESTNODEVALUES, queryString, optFilterRef, flags);
}

status_t NetworkTreeGateway :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * /*calledBy*/, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
printf("RequestNodeSubtrees [%s]\n", queryStrings.HeadWithDefault()());
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_REQUESTNODESUBTREES);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   const uint32 numQs = queryStrings.GetNumItems();
   for (uint32 i=0; i<numQs; i++)
   {
      if (msg()->AddString(NTG_NAME_PATH, queryStrings[i]).IsError(ret)) return ret;
      if ((i<queryFilters.GetNumItems())&&(queryFilters[i]())&&(msg()->AddArchiveMessage(NTG_NAME_QUERYFILTER, *queryFilters[i]()).IsError(ret))) return ret;
   }

   ret = msg()->CAddInt32(NTG_NAME_MAXDEPTH, maxDepth, MUSCLE_NO_LIMIT)
       | msg()->CAddFlat( NTG_NAME_FLAGS,    flags)
       | msg()->AddString(NTG_NAME_TAG,      tag);

   return ret.IsOK() ? AddOrSendOutgoingMessage(msg) : ret;
}

status_t NetworkTreeGateway :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
printf("UploadNodeValue [%s] %p [%s[\n", path(), optPayload(), optBefore);
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_UPLOADNODEVALUE);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   const status_t ret = msg()->CAddString( NTG_NAME_PATH,    path) 
                      | msg()->CAddMessage(NTG_NAME_PAYLOAD, optPayload)
                      | msg()->CAddFlat(   NTG_NAME_FLAGS,   flags)
                      | msg()->CAddString( NTG_NAME_BEFORE,  optBefore);

   return ret.IsOK() ? AddOrSendOutgoingMessage(msg) : ret;
}

status_t NetworkTreeGateway :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * /*calledBy*/, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
printf("UploadNodeSubtree [%s]\n", basePath());
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_UPLOADNODESUBTREE);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   const status_t ret = msg()->CAddString( NTG_NAME_PATH,    basePath) 
                      | msg()->CAddMessage(NTG_NAME_PAYLOAD, valuesMsg)
                      | msg()->CAddFlat(   NTG_NAME_FLAGS,   flags);

   return ret.IsOK() ? AddOrSendOutgoingMessage(msg) : ret;
}

status_t NetworkTreeGateway :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("RequestDeleteNodes [%s]\n", path());
   return HandleBasicCommandAux(NTG_COMMAND_REMOVENODES, path, optFilterRef, flags);
}

status_t NetworkTreeGateway :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const char * optBefore, TreeGatewayFlags flags)
{
printf("RequestMoveIndexEntry [%s]\n", path());
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_REORDERNODES);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   const status_t ret = msg()->CAddString(NTG_NAME_PATH,   path) 
                      | msg()->CAddString(NTG_NAME_BEFORE, optBefore)
                      | msg()->CAddFlat(  NTG_NAME_FLAGS, flags);

   return ret.IsOK() ? AddOrSendOutgoingMessage(msg) : ret;
}

status_t NetworkTreeGateway :: TreeGateway_PingServer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, TreeGatewayFlags flags)
{
printf("PingServer [%s]\n", tag());
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_PING);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if (msg()->CAddFlat(NTG_NAME_FLAGS, flags).IsError(ret)) return ret;
   return msg()->AddString(NTG_NAME_TAG, tag).IsOK(ret) ? AddOrSendOutgoingMessage(msg) : ret;
}

void NetworkTreeGateway :: SetNetworkConnected(bool isConnected)
{
printf("SetNetworkConnected %i\n", isConnected);
   if (isConnected != _isConnected)
   {
      _isConnected = isConnected;
      TreeGatewayConnectionStateChanged();
   }
}

status_t NetworkTreeGateway :: AddOrSendOutgoingMessage(const MessageRef & msgRef)
{
   if (msgRef() == NULL) return B_BAD_ARGUMENT;
   return IsInCommandBatch() ? AssembleBatchMessage(_outgoingBatchMsg, msgRef) : SendOutgoingMessageToNetwork(msgRef);
}

void NetworkTreeGateway :: CommandBatchEnds()
{
   ITreeGateway::CommandBatchEnds();
printf("CommandBatchEnds %p\n", _outgoingBatchMsg());
   if (_outgoingBatchMsg())
   {
      (void) SendOutgoingMessageToNetwork(_outgoingBatchMsg);
      _outgoingBatchMsg.Reset();
   }
}

status_t NetworkTreeGateway :: HandleBasicCommandAux(uint32 what, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   MessageRef msg = GetMessageFromPool(what);
   if (msg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if ((optFilterRef())&&(msg()->AddArchiveMessage(NTG_NAME_QUERYFILTER, *optFilterRef()).IsError(ret))) return ret;

   MessageRef queryFilterMsg;
   ret = (msg()->CAddString( NTG_NAME_PATH,  subscriptionPath)) |
         (msg()->CAddFlat(   NTG_NAME_FLAGS, flags));

   return ret.IsOK() ? AddOrSendOutgoingMessage(msg) : ret;
}

QueryFilterRef NetworkTreeGatewaySubscriber :: InstantiateQueryFilterAux(const Message & msg, uint32 which)
{
   MessageRef qfMsg;
   return (msg.FindMessage(NTG_NAME_QUERYFILTER, which, qfMsg).IsOK()) ? GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg()) : QueryFilterRef();
}

status_t NetworkTreeGatewaySubscriber :: IncomingTreeMessageReceivedFromClient(const MessageRef & msg)
{
printf("IncomingTreeMessageReceivedFromClient: "); msg()->PrintToStream();
   TreeGatewayFlags flags = msg()->GetFlat<TreeGatewayFlags>(NTG_NAME_FLAGS);
   QueryFilterRef qfRef   = InstantiateQueryFilterAux(*msg(), 0);
   const String & path    = *(msg()->GetStringPointer(NTG_NAME_PATH, &GetEmptyString()));
   const String & tag     = *(msg()->GetStringPointer(NTG_NAME_TAG,  &GetEmptyString()));
   MessageRef payload     = msg()->GetMessage(NTG_NAME_PAYLOAD);
   const char * optBefore = msg()->GetCstr(NTG_NAME_BEFORE);

   switch(msg()->what)
   {
      case NTG_COMMAND_ADDSUBSCRIPTION:    (void) AddTreeSubscription(      path, qfRef,     flags); break;
      case NTG_COMMAND_REMOVESUBSCRIPTION: (void) RemoveTreeSubscription(   path, qfRef,     flags); break;
      case NTG_COMMAND_REQUESTNODEVALUES:  (void) RequestTreeNodeValues(    path, qfRef,     flags); break;
      case NTG_COMMAND_REMOVENODES:        (void) RequestDeleteTreeNodes(   path, qfRef,     flags); break;
      case NTG_COMMAND_UPLOADNODESUBTREE:  (void) UploadTreeNodeSubtree(    path, payload,   flags); break;
      case NTG_COMMAND_REORDERNODES:       (void) RequestMoveTreeIndexEntry(path, optBefore, flags); break;
      case NTG_COMMAND_PING:               (void) PingTreeServer(                 tag,       flags); break;
      case NTG_COMMAND_UPLOADNODEVALUE:    (void) UploadTreeNodeValue(      path, payload,   flags, optBefore); break;

      case NTG_COMMAND_REQUESTNODESUBTREES:
      {
         Queue<String> queryStrings;
         Queue<ConstQueryFilterRef> queryFilters;
         const String * nextString;
         for (uint32 i=0; msg()->FindString(NTG_NAME_PATH, i, &nextString) == B_NO_ERROR; i++)
         {
            if (queryStrings.AddTail(*nextString) == B_NO_ERROR) 
            {
               QueryFilterRef nextQF = (i==0) ? qfRef : InstantiateQueryFilterAux(*msg(), i);
               if (nextQF()) (void) queryFilters.AddTail(nextQF);
            }
         }
         (void) RequestTreeNodeSubtrees(queryStrings, queryFilters, tag, msg()->GetInt32(NTG_NAME_MAXDEPTH, MUSCLE_NO_LIMIT), flags);
      }
      break;

      default:
         return B_UNIMPLEMENTED;  // unhandled/unknown Message type!
   }

   return B_NO_ERROR; // If we got here, the Message was handled
}

void NetworkTreeGatewaySubscriber :: TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg)
{
printf("TreeNodeUpdated [%s]\n", nodePath());
   MessageRef msg = GetMessageFromPool(NTG_REPLY_NODEUPDATED);
   if ((msg())&&(msg()->CAddString(NTG_NAME_PATH, nodePath).IsOK())&&(msg()->CAddMessage(NTG_NAME_PAYLOAD, payloadMsg).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void NetworkTreeGatewaySubscriber :: TreeNodeIndexCleared(const String & path)
{
printf("TreeNodeIndexCleared [%s]\n", path());
   MessageRef msg = GetMessageFromPool(NTG_REPLY_INDEXCLEARED);
   if ((msg())&&(msg()->CAddString(NTG_NAME_PATH, path).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void NetworkTreeGatewaySubscriber :: TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName)
{
printf("TreeNodeIndexEntryInserted [%s]\n", path());
   HandleIndexEntryUpdate(NTG_REPLY_INDEXENTRYINSERTED, path, insertedAtIndex, nodeName);
}

void NetworkTreeGatewaySubscriber :: TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName)
{
printf("TreeNodeIndexEntryRemoved [%s]\n", path());
   HandleIndexEntryUpdate(NTG_REPLY_INDEXENTRYREMOVED, path, removedAtIndex, nodeName);
}

void NetworkTreeGatewaySubscriber :: HandleIndexEntryUpdate(uint32 whatCode, const String & path, uint32 idx, const String & nodeName)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   if ((msg())&&(msg()->CAddString(NTG_NAME_PATH, path).IsOK())&&(msg()->CAddInt32(NTG_NAME_INDEX, idx).IsOK())&&(msg()->CAddString(NTG_NAME_NAME, nodeName).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void NetworkTreeGatewaySubscriber :: TreeServerPonged(const String & tag)
{
printf("TreeServerPonged [%s]\n", tag());
   MessageRef msg = GetMessageFromPool(NTG_REPLY_PONG);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, tag).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void NetworkTreeGatewaySubscriber :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
printf("SubtreesRequestResultReturned [%s] %p\n", tag(), subtreeData());
   MessageRef msg = GetMessageFromPool(NTG_REPLY_SUBTREES);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, tag).IsOK())&&(msg()->CAddMessage(NTG_NAME_PAYLOAD, subtreeData).IsOK())) SendOutgoingMessageToNetwork(msg);
}

status_t NetworkTreeGatewaySubscriber :: IncomingTreeMessageReceivedFromServer(const MessageRef & msg)
{
printf("IncomingTreeMessageReceivedFromServer: "); msg()->PrintToStream();
   const String & path = *(msg()->GetStringPointer(NTG_NAME_PATH, &GetEmptyString()));
   const String & tag  = *(msg()->GetStringPointer(NTG_NAME_TAG,  &GetEmptyString()));
   const String & name = *(msg()->GetStringPointer(NTG_NAME_NAME, &GetEmptyString()));
   MessageRef payload  = msg()->GetMessage(NTG_NAME_PAYLOAD);
   const int32 idx     = msg()->GetInt32(NTG_NAME_INDEX);
   
   switch(msg()->what)
   {
      case NTG_REPLY_NODEUPDATED:        TreeNodeUpdated(path, payload);              break;
      case NTG_REPLY_INDEXCLEARED:       TreeNodeIndexCleared(path);                  break;
      case NTG_REPLY_INDEXENTRYINSERTED: TreeNodeIndexEntryInserted(path, idx, name); break;
      case NTG_REPLY_INDEXENTRYREMOVED:  TreeNodeIndexEntryRemoved( path, idx, name); break;
      case NTG_REPLY_PONG:               TreeServerPonged(tag);                       break;
      case NTG_REPLY_SUBTREES:           SubtreesRequestResultReturned(tag, payload); break;

      default:
         return B_UNIMPLEMENTED;  // unhandled/unknown Message type!
   }

   return B_NO_ERROR; // If we got here, the Message was handled
}


}; // end namespace zg

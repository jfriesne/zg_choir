#include "zg/gateway/INetworkMessageSender.h"
#include "zg/messagetree/client/ClientSideNetworkTreeGateway.h"
#include "zg/messagetree/server/ServerSideNetworkTreeGatewaySubscriber.h"
#include "reflector/StorageReflectConstants.h"  // for PR_RESULT_*
#include "reflector/StorageReflectSession.h"    // for NODE_DEPTH_*
#include "regex/QueryFilter.h"          // for CreateQueryFilter()
#include "regex/PathMatcher.h"          // for GetPathDepth()
#include "util/MiscUtilityFunctions.h"  // for AssembleBatchMesssage()

namespace zg {

extern const String _opTagFieldName = "_op_fn";  // String field containing opTag strings referenced in the Message
extern const String _opTagPutMap    = "_op_pm";  // int32 field; values are (opTagIndex<<24)|(fieldNameIndex<<12)|(valueIndex)
extern const String _opTagRemoveMap = "_op_rm";  // int32 field; values are (opTagIndex)

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
   NTG_COMMAND_MOVEINDEXENTRIES,
   NTG_COMMAND_BEGINSEQUENCE,
   NTG_COMMAND_ENDSEQUENCE,
   NTG_COMMAND_UNDO,
   NTG_COMMAND_REDO,
   NTG_COMMAND_MESSAGETOSENIORPEER,
   NTG_COMMAND_MESSAGETOSUBSCRIBER,
};

// Reply-codes for Messages sent from server to client
enum {
   NTG_REPLY_NODEUPDATED = 1852273200,  // 'ngr0' 
   NTG_REPLY_INDEXCLEARED,
   NTG_REPLY_INDEXENTRYINSERTED,
   NTG_REPLY_INDEXENTRYREMOVED,
   NTG_REPLY_PONG,
   NTG_REPLY_SUBTREES,
   NTG_REPLY_MESSAGEFROMSENIORPEER,
   NTG_REPLY_MESSAGEFROMSUBSCRIBER
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

ClientSideNetworkTreeGateway :: ClientSideNetworkTreeGateway(INetworkMessageSender * messageSender)
   : ProxyTreeGateway(NULL)
   , _messageSender(messageSender)
   , _isConnected(false)
{
   // empty
}
   
ClientSideNetworkTreeGateway :: ~ClientSideNetworkTreeGateway()
{
   // empty
}

void ClientSideNetworkTreeGateway :: SetNetworkConnected(bool isConnected)
{
   if (isConnected != _isConnected)
   {
      _isConnected = isConnected;
      GatewayCallbackBatchGuard<ITreeGateway> gcbg(this);
      TreeGatewayConnectionStateChanged();
      if (_isConnected == false) SetParameters(MessageRef());  // no sense keeping parameters around from a TCP connection we no longer have
   }
}

status_t ClientSideNetworkTreeGateway :: SendOutgoingMessageToNetwork(const MessageRef & msgRef)
{
   if (msgRef() == NULL) return B_BAD_ARGUMENT;
   return IsInCommandBatch() ? AssembleBatchMessage(_outgoingBatchMsg, msgRef) : _messageSender->SendOutgoingMessageToNetwork(msgRef);
}

void ClientSideNetworkTreeGateway :: CommandBatchEnds()
{
   ProxyTreeGateway::CommandBatchEnds();
   if (_outgoingBatchMsg())
   {
      (void) _messageSender->SendOutgoingMessageToNetwork(_outgoingBatchMsg);
      _outgoingBatchMsg.Reset();
   }
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return HandleBasicCommandAux(NTG_COMMAND_ADDSUBSCRIPTION, subscriptionPath, optFilterRef, flags, GetEmptyString());
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * /*calledBy*/, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   return HandleBasicCommandAux(NTG_COMMAND_REMOVESUBSCRIPTION, subscriptionPath, optFilterRef, flags, GetEmptyString());
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * /*calledBy*/, TreeGatewayFlags flags)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_REMOVEALLSUBSCRIPTIONS);
   MRETURN_OOM_ON_NULL(msg());

   status_t ret;
   return msg()->CAddFlat(NTG_NAME_FLAGS, flags).IsOK(ret) ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * /*calledBy*/, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & tag)
{
   return HandleBasicCommandAux(NTG_COMMAND_REQUESTNODEVALUES, queryString, optFilterRef, flags, tag);
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * /*calledBy*/, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_REQUESTNODESUBTREES);
   MRETURN_OOM_ON_NULL(msg());

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

   return ret.IsOK() ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_UPLOADNODEVALUE);
   MRETURN_OOM_ON_NULL(msg());

   status_t ret = msg()->CAddString( NTG_NAME_PATH,    path)
                | msg()->CAddMessage(NTG_NAME_PAYLOAD, optPayload)
                | msg()->CAddFlat(   NTG_NAME_FLAGS,   flags)
                | msg()->CAddString( NTG_NAME_BEFORE,  optBefore)
                | msg()->CAddString( NTG_NAME_TAG,     optOpTag);

   return ret.IsOK() ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * /*calledBy*/, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_UPLOADNODESUBTREE);
   MRETURN_OOM_ON_NULL(msg());

   const status_t ret = msg()->CAddString( NTG_NAME_PATH,    basePath)
                      | msg()->CAddMessage(NTG_NAME_PAYLOAD, valuesMsg)
                      | msg()->CAddFlat(   NTG_NAME_FLAGS,   flags)
                      | msg()->CAddString( NTG_NAME_TAG,     optOpTag);

   return ret.IsOK() ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   return HandleBasicCommandAux(NTG_COMMAND_REMOVENODES, path, optFilterRef, flags, optOpTag);
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const String & optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_MOVEINDEXENTRIES);
   MRETURN_OOM_ON_NULL(msg());

   if (optFilterRef()) MRETURN_ON_ERROR(msg()->AddArchiveMessage(NTG_NAME_QUERYFILTER, *optFilterRef()));

   const status_t ret = msg()->CAddString(NTG_NAME_PATH,  path)
                      | msg()->CAddString(NTG_NAME_TAG,   optOpTag)
                      | msg()->CAddFlat(  NTG_NAME_FLAGS, flags)
                      | msg()->CAddString(NTG_NAME_BEFORE, optBefore);

   return ret.IsOK() ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, TreeGatewayFlags flags)
{
   return PingLocalPeerAux(tag, -1, flags);
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * /*calledBy*/, const String & tag, uint32 whichDB, TreeGatewayFlags flags)
{
   return PingLocalPeerAux(tag, whichDB, flags);
}

status_t ClientSideNetworkTreeGateway :: PingLocalPeerAux(const String & tag, int32 optWhichDB, TreeGatewayFlags flags)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_PING);
   MRETURN_OOM_ON_NULL(msg());

   status_t ret;
   if (msg()->CAddFlat(     NTG_NAME_FLAGS, flags).IsError(ret)) return ret;
   if (msg()->CAddInt32(    NTG_NAME_INDEX, optWhichDB).IsError(ret)) return ret;
   return msg()->CAddString(NTG_NAME_TAG,   tag).IsOK(ret) ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * /*calledBy*/, const MessageRef & userMsg, uint32 whichDB, const String & tag)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_MESSAGETOSENIORPEER);
   MRETURN_OOM_ON_NULL(msg());

   status_t ret;
   if (msg()->AddMessage(   NTG_NAME_PAYLOAD, userMsg).IsError(ret)) return ret;
   if (msg()->CAddInt32(    NTG_NAME_INDEX,   whichDB).IsError(ret)) return ret;
   return msg()->CAddString(NTG_NAME_TAG,     tag).IsOK(ret) ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway  :: TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * /*calledBy*/, const String & path, const MessageRef & userMsg, const ConstQueryFilterRef & optFilterRef, const String & tag)
{
   MessageRef msg = GetMessageFromPool(NTG_COMMAND_MESSAGETOSUBSCRIBER);
   MRETURN_OOM_ON_NULL(msg());

   status_t ret;
   if (msg()->CAddString(        NTG_NAME_PATH,        path).IsError(ret))         return ret;
   if (msg()->AddMessage(        NTG_NAME_PAYLOAD,     userMsg).IsError(ret))      return ret;
   if (msg()->CAddArchiveMessage(NTG_NAME_QUERYFILTER, optFilterRef).IsError(ret)) return ret;
   return msg()->CAddString(     NTG_NAME_TAG,         tag).IsOK(ret) ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return SendUndoRedoMessage(NTG_COMMAND_BEGINSEQUENCE, optSequenceLabel, whichDB);
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_EndUndoSequence(ITreeGatewaySubscriber * /*calledBy*/, const String & optSequenceLabel, uint32 whichDB)
{
   return SendUndoRedoMessage(NTG_COMMAND_ENDSEQUENCE, optSequenceLabel, whichDB);
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RequestUndo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB, const String & optOpTag)
{
   return SendUndoRedoMessage(NTG_COMMAND_UNDO, optOpTag, whichDB);
}

status_t ClientSideNetworkTreeGateway :: TreeGateway_RequestRedo(ITreeGatewaySubscriber * /*calledBy*/, uint32 whichDB, const String & optOpTag)
{
   return SendUndoRedoMessage(NTG_COMMAND_REDO, optOpTag, whichDB);
}

status_t ClientSideNetworkTreeGateway :: SendUndoRedoMessage(uint32 whatCode, const String & tag, uint32 whichDB)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   MRETURN_OOM_ON_NULL(msg());

   const status_t ret = msg()->CAddString(NTG_NAME_TAG, tag) | msg()->CAddInt32(NTG_NAME_INDEX, whichDB);
   return ret.IsOK() ? SendOutgoingMessageToNetwork(msg) : ret;
}

status_t ClientSideNetworkTreeGateway :: HandleBasicCommandAux(uint32 what, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(what);
   MRETURN_OOM_ON_NULL(msg());

   if (optFilterRef()) MRETURN_ON_ERROR(msg()->AddArchiveMessage(NTG_NAME_QUERYFILTER, *optFilterRef()));

   const status_t ret = (msg()->CAddString( NTG_NAME_PATH,  subscriptionPath))
                      | (msg()->CAddString( NTG_NAME_TAG,   optOpTag))
                      | (msg()->CAddFlat(   NTG_NAME_FLAGS, flags));

   return ret.IsOK() ? SendOutgoingMessageToNetwork(msg) : ret;
}

void ClientSideNetworkTreeGateway :: SetParameters(const MessageRef & parameters)
{
   _parameters = parameters;
}

ServerSideNetworkTreeGatewaySubscriber :: ServerSideNetworkTreeGatewaySubscriber(ITreeGateway * upstreamGateway, INetworkMessageSender * messageSender)
   : ITreeGatewaySubscriber(upstreamGateway)
   , _messageSender(messageSender)
{
   // empty
}

status_t ServerSideNetworkTreeGatewaySubscriber :: SendOutgoingMessageToNetwork(const MessageRef & msg) 
{
   return _messageSender->SendOutgoingMessageToNetwork(msg);
}

QueryFilterRef ServerSideNetworkTreeGatewaySubscriber :: InstantiateQueryFilterAux(const Message & msg, uint32 which)
{
   MessageRef qfMsg;
   return (msg.FindMessage(NTG_NAME_QUERYFILTER, which, qfMsg).IsOK()) ? GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg()) : QueryFilterRef();
}

status_t ServerSideNetworkTreeGatewaySubscriber :: IncomingTreeMessageReceivedFromClient(const MessageRef & msg)
{
   GatewaySubscriberCommandBatchGuard<ITreeGatewaySubscriber> cbg(this);  // let everyone know when this Message's processing begins and ends

   TreeGatewayFlags flags = msg()->GetFlat<TreeGatewayFlags>(NTG_NAME_FLAGS);
   QueryFilterRef qfRef   = InstantiateQueryFilterAux(*msg(), 0);
   const String & path    = msg()->GetStringReference(NTG_NAME_PATH);
   const String & optB4   = msg()->GetStringReference(NTG_NAME_BEFORE);
   const String & tag     = msg()->GetStringReference(NTG_NAME_TAG);
   MessageRef payload     = msg()->GetMessage(NTG_NAME_PAYLOAD);
   const int32 index      = msg()->GetInt32(NTG_NAME_INDEX);

   switch(msg()->what)
   {
      case NTG_COMMAND_ADDSUBSCRIPTION:    (void) AddTreeSubscription(      path, qfRef,   flags);             break;
      case NTG_COMMAND_REMOVESUBSCRIPTION: (void) RemoveTreeSubscription(   path, qfRef,   flags);             break;
      case NTG_COMMAND_REQUESTNODEVALUES:  (void) RequestTreeNodeValues(    path, qfRef,   flags, tag);        break;
      case NTG_COMMAND_REMOVENODES:        (void) RequestDeleteTreeNodes(   path, qfRef,   flags, tag);        break;
      case NTG_COMMAND_UPLOADNODESUBTREE:  (void) UploadTreeNodeSubtree(    path, payload, flags, tag);        break;
      case NTG_COMMAND_MOVEINDEXENTRIES:   (void) RequestMoveTreeIndexEntry(path, optB4,   qfRef, flags, tag); break;
      case NTG_COMMAND_UPLOADNODEVALUE:    (void) UploadTreeNodeValue(      path, payload, flags, optB4, tag); break;

      case NTG_COMMAND_PING:
      {
         if (index >= 0) (void) PingTreeSeniorPeer(tag, index, flags);
                    else (void) PingTreeLocalPeer( tag,      flags);
      }
      break;

      case NTG_COMMAND_REQUESTNODESUBTREES:
      {
         Queue<String> queryStrings;
         Queue<ConstQueryFilterRef> queryFilters;
         const String * nextString;
         for (uint32 i=0; msg()->FindString(NTG_NAME_PATH, i, &nextString).IsOK(); i++)
         {
            if (queryStrings.AddTail(*nextString).IsOK()) 
            {
               QueryFilterRef nextQF = (i==0) ? qfRef : InstantiateQueryFilterAux(*msg(), i);
               if (nextQF()) (void) queryFilters.AddTail(nextQF);
            }
         }
         (void) RequestTreeNodeSubtrees(queryStrings, queryFilters, tag, msg()->GetInt32(NTG_NAME_MAXDEPTH, MUSCLE_NO_LIMIT), flags);
      }
      break;

      case NTG_COMMAND_BEGINSEQUENCE: (void) BeginUndoSequence(tag, index); break;
      case NTG_COMMAND_ENDSEQUENCE:   (void) EndUndoSequence(  tag, index); break;
      case NTG_COMMAND_UNDO:          (void) RequestUndo(index, tag);       break;
      case NTG_COMMAND_REDO:          (void) RequestRedo(index, tag);       break;

      case NTG_COMMAND_MESSAGETOSENIORPEER:
         (void) SendMessageToTreeSeniorPeer(payload, index, tag);
      break;

      case NTG_COMMAND_MESSAGETOSUBSCRIBER:
         (void) SendMessageToSubscriber(path, payload, qfRef, tag);
      break;

      default:
         return B_UNIMPLEMENTED;  // unhandled/unknown Message type!
   }

   return B_NO_ERROR; // If we got here, the Message was handled
}

void ServerSideNetworkTreeGatewaySubscriber :: TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_NODEUPDATED);
   if ((msg())
     &&(msg()->CAddString( NTG_NAME_PATH,      nodePath).IsOK())
     &&(msg()->CAddString( NTG_NAME_TAG,       optOpTag).IsOK())
     &&(msg()->CAddMessage(NTG_NAME_PAYLOAD, payloadMsg).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: TreeNodeIndexCleared(const String & path, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_INDEXCLEARED);
   if ((msg())
     &&(msg()->CAddString(NTG_NAME_PATH,     path).IsOK())
     &&(msg()->CAddString(NTG_NAME_TAG,  optOpTag).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag)
{
   HandleIndexEntryUpdate(NTG_REPLY_INDEXENTRYINSERTED, path, insertedAtIndex, nodeName, optOpTag);
}

void ServerSideNetworkTreeGatewaySubscriber :: TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag)
{
   HandleIndexEntryUpdate(NTG_REPLY_INDEXENTRYREMOVED, path, removedAtIndex, nodeName, optOpTag);
}

void ServerSideNetworkTreeGatewaySubscriber :: HandleIndexEntryUpdate(uint32 whatCode, const String & path, uint32 idx, const String & nodeName, const String & optOpTag)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   if ((msg())
     &&(msg()->CAddString(NTG_NAME_PATH,     path).IsOK())
     &&(msg()->CAddString(NTG_NAME_TAG,  optOpTag).IsOK())
     &&(msg()->CAddInt32(NTG_NAME_INDEX,      idx).IsOK())
     &&(msg()->CAddString(NTG_NAME_NAME, nodeName).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: TreeLocalPeerPonged(const String & tag)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_PONG);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, tag).IsOK())&&(msg()->AddInt32(NTG_NAME_INDEX, -1).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: TreeSeniorPeerPonged(const String & tag, uint32 whichDB)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_PONG);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, tag).IsOK())&&(msg()->CAddInt32(NTG_NAME_INDEX, whichDB).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: MessageReceivedFromTreeSeniorPeer(int32 whichDB, const String & tag, const MessageRef & payload)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_MESSAGEFROMSENIORPEER);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, tag).IsOK())&&(msg()->CAddInt32(NTG_NAME_INDEX, whichDB).IsOK())&&(msg()->AddMessage(NTG_NAME_PAYLOAD, payload).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_MESSAGEFROMSUBSCRIBER);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, returnAddress).IsOK())&&(msg()->CAddString(NTG_NAME_PATH, nodePath).IsOK())&&(msg()->AddMessage(NTG_NAME_PAYLOAD, payload).IsOK())) SendOutgoingMessageToNetwork(msg);
}

void ServerSideNetworkTreeGatewaySubscriber :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   MessageRef msg = GetMessageFromPool(NTG_REPLY_SUBTREES);
   if ((msg())&&(msg()->CAddString(NTG_NAME_TAG, tag).IsOK())&&(msg()->CAddMessage(NTG_NAME_PAYLOAD, subtreeData).IsOK())) SendOutgoingMessageToNetwork(msg);
}

status_t ClientSideNetworkTreeGateway :: IncomingTreeMessageReceivedFromServer(const MessageRef & msg)
{
   GatewayCallbackBatchGuard<ITreeGateway> cbg(this);  // let everyone know when this Message's processing begins and ends
   if (muscleInRange(msg()->what, (uint32)BEGIN_PR_RESULTS, (uint32)END_PR_RESULTS)) return IncomingMuscledMessageReceivedFromServer(msg);

   const String & path = msg()->GetStringReference(NTG_NAME_PATH);
   const String & tag  = msg()->GetStringReference(NTG_NAME_TAG);
   const String & name = msg()->GetStringReference(NTG_NAME_NAME);
   MessageRef payload  = msg()->GetMessage(NTG_NAME_PAYLOAD);
   const int32 idx     = msg()->GetInt32(NTG_NAME_INDEX);
   
   switch(msg()->what)
   {
      case NTG_REPLY_NODEUPDATED:        TreeNodeUpdated(path, payload, tag);              break;
      case NTG_REPLY_INDEXCLEARED:       TreeNodeIndexCleared(path, tag);                  break;
      case NTG_REPLY_INDEXENTRYINSERTED: TreeNodeIndexEntryInserted(path, idx, name, tag); break;
      case NTG_REPLY_INDEXENTRYREMOVED:  TreeNodeIndexEntryRemoved( path, idx, name, tag); break;
      case NTG_REPLY_SUBTREES:           SubtreesRequestResultReturned(tag, payload);      break;

      case NTG_REPLY_PONG:
         if (idx >= 0) TreeSeniorPeerPonged(tag, idx);
                  else TreeLocalPeerPonged(tag);
      break;

      case NTG_REPLY_MESSAGEFROMSENIORPEER: 
         MessageReceivedFromTreeSeniorPeer(idx, tag, payload);
      break;

      case NTG_REPLY_MESSAGEFROMSUBSCRIBER: 
         MessageReceivedFromSubscriber(path, payload, tag);
      break;

      default:
         return B_UNIMPLEMENTED;  // unhandled/unknown Message type!
   }

   return B_NO_ERROR; // If we got here, the Message was handled
}

status_t ClientSideNetworkTreeGateway :: ConvertPathToSessionRelative(String & path) const
{
   if (GetPathDepth(path()) >= NODE_DEPTH_USER)
   {
      path = GetPathClause(NODE_DEPTH_USER, path());
      return B_NO_ERROR;
   }
   return B_BAD_ARGUMENT;
}

static const String & LookupOpTagInPutMap(const Message & msg, const uint32 * opTagPutMap, uint32 opTagPutMapLength, uint32 currentFieldNameIndex, uint32 currentValueIndex)
{
   const String * ret = NULL;
   if (opTagPutMap)
   {
      for (uint32 j=0; j<opTagPutMapLength; j++)
      {
         const uint32 mapEntry     = opTagPutMap[j];
         const uint32 fieldNameIdx = (mapEntry>>12) & 0xFFF;
         if (fieldNameIdx == currentFieldNameIndex)
         {
            const uint32 valueIdx = mapEntry & 0xFFF;
            if (valueIdx == currentValueIndex) {ret = msg.GetStringPointer(_opTagFieldName, NULL, (mapEntry>>24)&0xFFF); break;}
         }
      }
   }
   return ret ? *ret : GetEmptyString();
}

// Special handling for PR_RESULT_* values, for cases where it's more convenient to have the server
// return results in that form than to use our internal NTG_REPLY_* format.
status_t ClientSideNetworkTreeGateway :: IncomingMuscledMessageReceivedFromServer(const MessageRef & msg)
{
   switch(msg()->what)
   {
      case PR_RESULT_DATATREES:
      {  
         String tag;
         if (msg()->FindString(PR_NAME_TREE_REQUEST_ID, tag).IsOK())
         {
            MessageRef sessionRelativeMsg = GetMessageFromPool();
            if (sessionRelativeMsg())
            {  
               // Convert the absolute paths back into user-friendly relative paths
               for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
               {  
                  String sessionRelativeString = iter.GetFieldName();
                  if (ConvertPathToSessionRelative(sessionRelativeString).IsOK()) (void) msg()->ShareName(iter.GetFieldName(), *sessionRelativeMsg(), sessionRelativeString);
               }
               SubtreesRequestResultReturned(tag, sessionRelativeMsg);
            }
         }
      }
      break;

      case PR_RESULT_DATAITEMS:
      {
         const bool hasOpTags = msg()->HasName(_opTagFieldName, B_STRING_TYPE);

         const uint32 * opTagPutMap = NULL;
         uint32 opTagPutMapLength = 0;
         if ((hasOpTags)&&(msg()->GetInfo(_opTagPutMap, NULL, &opTagPutMapLength).IsOK())) (void) msg()->FindData(_opTagPutMap, B_INT32_TYPE, (const void **) &opTagPutMap, NULL);

         // Handle notifications of removed nodes
         {
            String nodePath;
            for (int i=0; msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, nodePath).IsOK(); i++)
               if (ConvertPathToSessionRelative(nodePath).IsOK())
                  TreeNodeUpdated(nodePath, MessageRef(), hasOpTags ? msg()->GetStringReference(_opTagFieldName, msg()->GetInt32(_opTagRemoveMap, -1, i)) : GetEmptyString());
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
                     TreeNodeUpdated(nodePath, nodeRef, LookupOpTagInPutMap(*msg(), opTagPutMap, opTagPutMapLength, currentFieldNameIndex, i));
            }
         }
      }
      break;

      case PR_RESULT_INDEXUPDATED:
      {
         const bool hasOpTags = msg()->HasName(_opTagFieldName, B_STRING_TYPE);

         const uint32 * opTagPutMap = NULL;
         uint32 opTagPutMapLength = 0;
         if ((hasOpTags)&&(msg()->GetInfo(_opTagPutMap, NULL, &opTagPutMapLength).IsOK())) (void) msg()->FindData(_opTagPutMap, B_INT32_TYPE, (const void **) &opTagPutMap, NULL);

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
                  const String & optOpTag = LookupOpTagInPutMap(*msg(), opTagPutMap, opTagPutMapLength, currentFieldNameIndex, i);
                  const char * colonAt = strchr(indexCmd, ':');
                  char c = indexCmd[0];
                  switch(c)
                  {
                     case INDEX_OP_CLEARED:       TreeNodeIndexCleared(sessionRelativePath, optOpTag);                                                   break;
                     case INDEX_OP_ENTRYINSERTED: TreeNodeIndexEntryInserted(sessionRelativePath, atoi(&indexCmd[1]), colonAt?(colonAt+1):"", optOpTag); break;
                     case INDEX_OP_ENTRYREMOVED:  TreeNodeIndexEntryRemoved(sessionRelativePath,  atoi(&indexCmd[1]), colonAt?(colonAt+1):"", optOpTag); break;
                  }
               }
            }
         }
      }
      break;

      case PR_RESULT_PONG:
         TreeLocalPeerPonged(msg()->GetString(NTG_NAME_TAG));
      break;

      default:
         return B_UNIMPLEMENTED;  // unhandled/unknowm Message type!
   }

   return B_NO_ERROR;
}

MessageRef GetPongForLocalSyncPing(const Message & maybeSyncPingMsg)
{
   if ((maybeSyncPingMsg.what == NTG_COMMAND_PING)&&(maybeSyncPingMsg.GetFlat<TreeGatewayFlags>(NTG_NAME_FLAGS).IsBitSet(TREE_GATEWAY_FLAG_SYNC)))
   {
      // short-circuit the ping on the client-side, to support local-output-queue-drained-notifications
      MessageRef pongMsg = GetLightweightCopyOfMessageFromPool(maybeSyncPingMsg);
      if (pongMsg())
      {
         pongMsg()->what = NTG_REPLY_PONG;
         return pongMsg;
      }
   }
   return MessageRef();
}

}; // end namespace zg

#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/gateway/TreeConstants.h"  // for TREE_COMMAND_SETUNDOKEY

namespace zg {

// These objects are stored in NetworkTreeGateway.cpp but we want to reference them here also
extern const String _opTagFieldName;
extern const String _opTagPutMap;
extern const String _opTagRemoveMap;
extern const String _opTagDummy;

ServerSideMessageTreeSession :: ServerSideMessageTreeSession(ITreeGateway * upstreamGateway)
   : ServerSideNetworkTreeGatewaySubscriber(upstreamGateway, this)
   , _undoKey("anon")
   , _dbSession(NULL)
{
   // empty
}

ServerSideMessageTreeSession :: ~ServerSideMessageTreeSession()
{
   // empty
}

status_t ServerSideMessageTreeSession :: AttachedToServer()
{
   MRETURN_ON_ERROR(StorageReflectSession::AttachedToServer());

   _dbSession = FindFirstSessionOfType<MessageTreeDatabasePeerSession>();
   if (_logOnAttachAndDetach) LogTime(MUSCLE_LOG_INFO, "ServerSideMessageTreeSession %p:  Client at [%s] has connected to this server.\n", this, GetSessionRootPath()());
   return B_NO_ERROR;
}

void ServerSideMessageTreeSession :: AboutToDetachFromServer()
{
   if (_logOnAttachAndDetach) LogTime(MUSCLE_LOG_INFO, "ServerSideMessageTreeSession %p:  Client at [%s] has disconnected from this server.\n", this, GetSessionRootPath()());

   MessageTreeDatabasePeerSession * peerSession = FindFirstSessionOfType<MessageTreeDatabasePeerSession>();  // doing a fresh lookup to avoid shutdown-order-of-operations problems
   if (peerSession) peerSession->ServerSideMessageTreeSessionIsDetaching(this);  // notify the ZGPeer so that any ClientDataMessageTreeDatabaseObjects can remove our shared nodes

   StorageReflectSession::AboutToDetachFromServer();
}

void ServerSideMessageTreeSession :: MessageReceivedFromGateway(const MessageRef & msg, void * userData)
{
   NestCountGuard ncg(_isInMessageReceivedFromGateway);
   if (IncomingTreeMessageReceivedFromClient(msg) == B_UNIMPLEMENTED) 
   {
      switch(msg()->what)
      {
         case TREE_COMMAND_SETUNDOKEY:
            _undoKey = msg()->GetString(TREE_NAME_UNDOKEY);
         break;

         default:
            StorageReflectSession::MessageReceivedFromGateway(msg, userData);
         break;
      }
   }
}

status_t ServerSideMessageTreeSession :: AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleSubscribeMessage(subscriptionPath, optFilterRef, flags, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & /*optFilterRef*/, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleUnsubscribeMessage(subscriptionPath, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RemoveAllTreeSubscriptions(TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleUnsubscribeAllMessage(cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleRequestNodeValuesMessage(queryString, optFilterRef, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags /*flags*/)
{
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleRequestNodeSubtreesMessage(queryStrings, queryFilters, tag, maxDepth, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

void ServerSideMessageTreeSession :: AddApplicationSpecificParametersToParametersResultMessage(Message & parameterResultsMsg) const
{
   StorageReflectSession::AddApplicationSpecificParametersToParametersResultMessage(parameterResultsMsg);

   const ZGPeerSession * zps = FindFirstSessionOfType<ZGPeerSession>();
   if (zps)
   {
      const ZGPeerSettings & settings = zps->GetPeerSettings();
      (void) parameterResultsMsg.AddString(ZG_PARAMETER_NAME_PEERID,       zps->GetLocalPeerID().ToString());
      (void) parameterResultsMsg.AddString(ZG_PARAMETER_NAME_SIGNATURE,    settings.GetSignature());
      (void) parameterResultsMsg.AddString(ZG_PARAMETER_NAME_SYSTEMNAME,   settings.GetSystemName());
      (void) parameterResultsMsg.CAddMessage(ZG_PARAMETER_NAME_ATTRIBUTES, CastAwayConstFromRef(settings.GetPeerAttributes()));
      (void) parameterResultsMsg.AddInt8(ZG_PARAMETER_NAME_NUMDBS,         settings.GetNumDatabases());
   }
}

ServerSideMessageTreeSessionFactory :: ServerSideMessageTreeSessionFactory(ITreeGateway * upstreamGateway, bool announceClientConnectsAndDisconnects)
   : ITreeGatewaySubscriber(upstreamGateway)
   , _announceClientConnectsAndDisconnects(announceClientConnectsAndDisconnects)
{
   // empty
}

AbstractReflectSessionRef ServerSideMessageTreeSessionFactory :: CreateSession(const String & /*clientAddress*/, const IPAddressAndPort & /*factoryInfo*/)
{
   ServerSideMessageTreeSessionRef ret(newnothrow ServerSideMessageTreeSession(GetGateway()));
   if (ret()) 
   {
      ret()->SetLogOnAttachAndDetach(_announceClientConnectsAndDisconnects);
   }
   else MWARN_OUT_OF_MEMORY;

   return ret;
}

static status_t GetOrPutOpTagIndex(Message & subscriptionMessage, const String & optOpTag, int & opTagIndex)
{
   int32 arrayLen = 0;

   const String * nextStr;
   for (; subscriptionMessage.FindString(_opTagFieldName, arrayLen, &nextStr).IsOK(); arrayLen++)
   {  
      if (*nextStr == optOpTag)
      {  
         opTagIndex = arrayLen;
         return B_NO_ERROR;
      }
   }

   opTagIndex = arrayLen;
   return subscriptionMessage.AddString(_opTagFieldName, optOpTag);
}

status_t ServerSideMessageTreeSession :: UpdateSubscriptionMessage(Message & subscriptionMessage, const String & nodePath, const MessageRef & optMessageData)
{
   int32 opTagIndex = -1;  // this will be set to the index of our optOptTag string within the _opTagFieldName field
   const String & optOpTag = _dbSession ? _dbSession->GetCurrentOpTagForNodePath(nodePath) : GetEmptyString();
   if (optOpTag.HasChars()) MRETURN_ON_ERROR(GetOrPutOpTagIndex(subscriptionMessage, optOpTag, opTagIndex));

   MRETURN_ON_ERROR(StorageReflectSession::UpdateSubscriptionMessage(subscriptionMessage, nodePath, optMessageData));

   if (opTagIndex >= 0)
   {  
      if (optMessageData())
      {  
         // Harder case:  for each Message placed, we need to store its (fieldIndex,valueIndex) along with our own (opTagIndex)
         uint32 numValuesInField = 0;
         MRETURN_ON_ERROR(subscriptionMessage.GetInfo(nodePath, NULL, &numValuesInField));
         
         // if there's just 1 value in our field, then the field must have just been created just now, and therefore it's the last field-name in the list
         const uint32 fieldNameIndex = (numValuesInField > 1) ? subscriptionMessage.IndexOfName(nodePath) : (subscriptionMessage.GetNumNames()-1);
         MRETURN_ON_ERROR(subscriptionMessage.AddInt32(_opTagPutMap, (opTagIndex<<24)|(fieldNameIndex<<12)|(numValuesInField-1)));
      }   
      else 
      {
         uint32 removeMapLength = 0, dataItemsLength = 0;
         (void) subscriptionMessage.GetInfo(_opTagRemoveMap,           NULL, &removeMapLength);
         (void) subscriptionMessage.GetInfo(PR_NAME_REMOVED_DATAITEMS, NULL, &dataItemsLength);
         for (uint32 i=removeMapLength; (i+1)<dataItemsLength; i++) MRETURN_ON_ERROR(subscriptionMessage.AddInt32(_opTagRemoveMap, -1));  // in case optag-free items were previously addded
         MRETURN_ON_ERROR(subscriptionMessage.ReplaceInt32(true, _opTagRemoveMap, dataItemsLength-1, opTagIndex)); // since PR_NAME_REMOVED_DATAITEMS is just a single list we only need to record the (opTagIndex) each item corresponds to
      }
   }
   
   return B_NO_ERROR;
}

status_t ServerSideMessageTreeSession :: UpdateSubscriptionIndexMessage(Message & subscriptionIndexMessage, const String & nodePath, char op, uint32 index, const String & key)
{
   int32 opTagIndex = -1;  // this will be set to the index of our optOptTag string within the _opTagFieldName field
   const String & optOpTag = _dbSession ? _dbSession->GetCurrentOpTagForNodePath(nodePath) : GetEmptyString();
   if (optOpTag.HasChars()) MRETURN_ON_ERROR(GetOrPutOpTagIndex(subscriptionIndexMessage, optOpTag, opTagIndex));

   MRETURN_ON_ERROR(StorageReflectSession::UpdateSubscriptionIndexMessage(subscriptionIndexMessage, nodePath, op, index, key));

   if (opTagIndex >= 0)
   {  
      // Harder case:  for each String placed, we need to store its (fieldIndex,valueIndex) along with our own (opTagIndex)
      uint32 numValuesInField = 0;
      MRETURN_ON_ERROR(subscriptionIndexMessage.GetInfo(nodePath, NULL, &numValuesInField));
      
      // if there's just 1 value in our field, then the field must have just been created just now, and therefore it's the last field-name in the list
      const uint32 fieldNameIndex = (numValuesInField > 1) ? subscriptionIndexMessage.IndexOfName(nodePath) : (subscriptionIndexMessage.GetNumNames()-1);
      MRETURN_ON_ERROR(subscriptionIndexMessage.AddInt32(_opTagPutMap, (opTagIndex<<24)|(fieldNameIndex<<12)|(numValuesInField-1)));
   }

   return B_NO_ERROR;
}

status_t ServerSideMessageTreeSession :: PruneSubscriptionMessage(Message & subscriptionMessage, const String & nodePath)
{
   const int32 nodePathIndex = subscriptionMessage.HasName(_opTagPutMap, B_STRING_TYPE) ? subscriptionMessage.IndexOfName(nodePath) : -1;
   if (nodePathIndex >= 0)
   {
      uint32 nextEntry = 0;
      for (uint32 i=0; subscriptionMessage.FindInt32(_opTagPutMap, i, nextEntry).IsOK(); i++)
      { 
         const uint32 opTagIndex     = (nextEntry >> 24) & 0xFFF;
         const uint32 fieldNameIndex = (nextEntry >> 12) & 0xFFF;
         const uint32 valueIndex     = (nextEntry >> 00) & 0xFFF;

              if (fieldNameIndex == nodePathIndex) (void) subscriptionMessage.RemoveData(_opTagPutMap, i--);
         else if (fieldNameIndex  > nodePathIndex) (void) subscriptionMessage.ReplaceInt32(false, _opTagPutMap, i, (opTagIndex<<24)|((fieldNameIndex-1)<<12)|(valueIndex));
      }
   }

   return StorageReflectSession::PruneSubscriptionMessage(subscriptionMessage, nodePath);
}

};

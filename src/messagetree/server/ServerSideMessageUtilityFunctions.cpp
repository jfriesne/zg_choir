#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"
#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_*
#include "regex/StringMatcher.h"  // for EscapeRegexTokens()

namespace zg
{

static status_t CreateMuscleSubscribeMessageAux(uint32 whatCode, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(whatCode);
   if (retMsg() == NULL) RETURN_OUT_OF_MEMORY;

   const String pathArg = ((whatCode == PR_COMMAND_SETPARAMETERS) ? subscriptionPath : EscapeRegexTokens(subscriptionPath)).Prepend("SUBSCRIBE:");  // don't accidentally remove multiple subscriptions due to wildcarding!
   return (optFilterRef() ? retMsg()->CAddArchiveMessage(pathArg, optFilterRef) : retMsg()->AddBool(pathArg, true)) | retMsg()->CAddBool(PR_NAME_SUBSCRIBE_QUIETLY, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
}

status_t CreateMuscleSubscribeMessage(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, MessageRef & retMsg)
{
   return CreateMuscleSubscribeMessageAux(PR_COMMAND_SETPARAMETERS, subscriptionPath, optFilterRef, flags, retMsg);
}

status_t CreateMuscleUnsubscribeMessage(const String & subscriptionPath, MessageRef & retMsg)
{
   return CreateMuscleSubscribeMessageAux(PR_COMMAND_REMOVEPARAMETERS, subscriptionPath, ConstQueryFilterRef(), TreeGatewayFlags(), retMsg);
}

status_t CreateMuscleUnsubscribeAllMessage(MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
   if (retMsg() == NULL) RETURN_OUT_OF_MEMORY;

   return retMsg()->AddBool("SUBSCRIBE:*", true);
}

status_t CreateMuscleRequestNodeValuesMessage(const String & queryString, const ConstQueryFilterRef & optFilterRef, MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(PR_COMMAND_GETDATA);
   if (retMsg() == NULL) RETURN_OUT_OF_MEMORY;

   return retMsg()->AddString(PR_NAME_KEYS, queryString) 
        | retMsg()->CAddArchiveMessage(PR_NAME_FILTERS, optFilterRef);
}

status_t CreateMuscleRequestNodeSubtreesMessage(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, MessageRef & retMsg)
{
printf("ZG RequestNodeSubtrees [%s]\n", queryStrings.HeadWithDefault()());
   retMsg = GetMessageFromPool(PR_COMMAND_GETDATATREES);
   if (retMsg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   const uint32 numQueryStrings = queryStrings.GetNumItems();
   for (uint32 i=0; i<numQueryStrings; i++)
   {
      if (retMsg()->AddString(PR_NAME_KEYS, queryStrings[i]).IsError(ret)) return ret;
      if ((i<queryFilters.GetNumItems())&&(retMsg()->CAddArchiveMessage(PR_NAME_FILTERS, queryFilters[i]).IsError(ret))) return ret;
   }

   return retMsg()->CAddInt32(PR_NAME_MAXDEPTH, maxDepth, MUSCLE_NO_LIMIT) 
        | retMsg()->AddString(PR_NAME_TREE_REQUEST_ID, tag);
}

};  // end namespace zg

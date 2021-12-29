#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"
#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_*
#include "regex/StringMatcher.h"  // for EscapeRegexTokens()

namespace zg
{

status_t CreateMuscleSubscribeMessage(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   MRETURN_OOM_ON_NULL(retMsg());

   const String pathArg = subscriptionPath.Prepend("SUBSCRIBE:");
   return (optFilterRef() ? retMsg()->CAddArchiveMessage(pathArg, optFilterRef) : retMsg()->AddBool(pathArg, true)) 
        | (retMsg()->CAddBool(PR_NAME_SUBSCRIBE_QUIETLY, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY)));
}

status_t CreateMuscleUnsubscribeMessage(const String & subscriptionPath, MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
   MRETURN_OOM_ON_NULL(retMsg());

   return retMsg()->AddString(PR_NAME_KEYS, EscapeRegexTokens(subscriptionPath).Prepend("SUBSCRIBE:"));  // EscapeRegexTokens() is here to avoid accidentally removing other subscription-paths that the wildcards happen to match to
}

status_t CreateMuscleUnsubscribeAllMessage(MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
   MRETURN_OOM_ON_NULL(retMsg());

   return retMsg()->AddString(PR_NAME_KEYS, "SUBSCRIBE:*");
}

status_t CreateMuscleRequestNodeValuesMessage(const String & queryString, const ConstQueryFilterRef & optFilterRef, MessageRef & retMsg, const String & tag)
{
   retMsg = GetMessageFromPool(PR_COMMAND_GETDATA);
   MRETURN_OOM_ON_NULL(retMsg());

   return retMsg()->AddString(         PR_NAME_KEYS,            queryString)
        | retMsg()->CAddArchiveMessage(PR_NAME_FILTERS,         optFilterRef)
        | retMsg()->AddString(         PR_NAME_TREE_REQUEST_ID, tag);
}

status_t CreateMuscleRequestNodeSubtreesMessage(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, MessageRef & retMsg)
{
   retMsg = GetMessageFromPool(PR_COMMAND_GETDATATREES);
   MRETURN_OOM_ON_NULL(retMsg());

   const uint32 numQueryStrings = queryStrings.GetNumItems();
   for (uint32 i=0; i<numQueryStrings; i++)
   {
      MRETURN_ON_ERROR(retMsg()->AddString(PR_NAME_KEYS, queryStrings[i]));
      if (i<queryFilters.GetNumItems()) MRETURN_ON_ERROR(retMsg()->CAddArchiveMessage(PR_NAME_FILTERS, queryFilters[i]));
   }

   return retMsg()->CAddInt32(PR_NAME_MAXDEPTH,        maxDepth, MUSCLE_NO_LIMIT)
        | retMsg()->AddString(PR_NAME_TREE_REQUEST_ID, tag);
}

};  // end namespace zg

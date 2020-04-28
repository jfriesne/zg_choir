#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"

namespace zg {

ServerSideMessageTreeSession :: ServerSideMessageTreeSession(ITreeGateway * upstreamGateway)
   : ServerSideNetworkTreeGatewaySubscriber(upstreamGateway, this)
{
   // empty
}

ServerSideMessageTreeSession :: ~ServerSideMessageTreeSession()
{
   // empty
}

void ServerSideMessageTreeSession :: MessageReceivedFromGateway(const MessageRef & msg, void * userData)
{
   if (IncomingTreeMessageReceivedFromClient(msg) == B_UNIMPLEMENTED) StorageReflectSession::MessageReceivedFromGateway(msg, userData);
}

status_t ServerSideMessageTreeSession :: AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("SSMTSF:  AddTreeSubscription [%s]\n", subscriptionPath());
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleSubscribeMessage(subscriptionPath, optFilterRef, flags, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("SSMTSF:  RemoveTreeSubscription [%s]\n", subscriptionPath());
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleUnsubscribeMessage(subscriptionPath, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RemoveAllTreeSubscriptions(TreeGatewayFlags flags)
{
printf("SSMTSF:  RemoveAllTreeSubscriptions()\n");
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleUnsubscribeAllMessage(cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RequestTreeNodeValues(const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
printf("SSMTSF:  RequestTreeNodeValues\n");
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleRequestNodeValuesMessage(queryString, optFilterRef, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

status_t ServerSideMessageTreeSession :: RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
printf("SSMTSF:  RequestTreeNodeSubtrees\n");
   MessageRef cmdMsg;
   const status_t ret = CreateMuscleRequestNodeSubtreesMessage(queryStrings, queryFilters, tag, maxDepth, cmdMsg);
   if (ret.IsOK()) MessageReceivedFromGateway(cmdMsg, NULL);
   return ret;
}

ServerSideMessageTreeSessionFactory :: ServerSideMessageTreeSessionFactory(ITreeGateway * upstreamGateway)
   : ITreeGatewaySubscriber(upstreamGateway)
{
   // empty
}

AbstractReflectSessionRef ServerSideMessageTreeSessionFactory :: CreateSession(const String & /*clientAddress*/, const IPAddressAndPort & /*factoryInfo*/)
{
   ServerSideMessageTreeSessionRef ret(newnothrow ServerSideMessageTreeSession(GetGateway()));
   if (ret() == NULL) WARN_OUT_OF_MEMORY;
   return ret;
}

};

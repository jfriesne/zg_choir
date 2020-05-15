#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"

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

status_t ServerSideMessageTreeSession :: AttachedToServer()
{
   status_t ret = StorageReflectSession::AttachedToServer();
   if (ret.IsError()) return ret;

   if (_logOnAttachAndDetach) LogTime(MUSCLE_LOG_INFO, "ServerSideMessageTreeSession %p:  Client at [%s] has connected to this server.\n", this, GetSessionRootPath()());
   return ret;
}

void ServerSideMessageTreeSession :: AboutToDetachFromServer()
{
   if (_logOnAttachAndDetach) LogTime(MUSCLE_LOG_INFO, "ServerSideMessageTreeSession %p:  Client at [%s] has disconnected from this server.\n", this, GetSessionRootPath()());

   MessageTreeDatabasePeerSession * peerSession = FindFirstSessionOfType<MessageTreeDatabasePeerSession>();
   if (peerSession) peerSession->ServerSideMessageTreeSessionIsDetaching(this);  // notify the ZGPeer so that any ClientDataMessageTreeDatabaseObjects can remove our shared nodes

   StorageReflectSession::AboutToDetachFromServer();
}

void ServerSideMessageTreeSession :: MessageReceivedFromGateway(const MessageRef & msg, void * userData)
{
   NestCountGuard ncg(_isInMessageReceivedFromGateway);
   if (IncomingTreeMessageReceivedFromClient(msg) == B_UNIMPLEMENTED) StorageReflectSession::MessageReceivedFromGateway(msg, userData);
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
   else WARN_OUT_OF_MEMORY;

   return ret;
}

};

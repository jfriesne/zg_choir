#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "zg/messagetree/server/ServerSideMessageUtilityFunctions.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/gateway/TreeConstants.h"  // for TREE_COMMAND_SETUNDOKEY

namespace zg {

ServerSideMessageTreeSession :: ServerSideMessageTreeSession(ITreeGateway * upstreamGateway)
   : ServerSideNetworkTreeGatewaySubscriber(upstreamGateway, this)
   , _undoKey("anon")
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

};

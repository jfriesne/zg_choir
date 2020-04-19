#include "zg/messagetree/server/ServerSideMessageTreeSession.h"

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

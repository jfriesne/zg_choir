#include "zg/gateway/tree/TreeServerSideSession.h"

namespace zg {

TreeServerSideSession :: TreeServerSideSession(ITreeGateway * upstreamGateway)
   : ServerSideNetworkTreeGatewaySubscriber(upstreamGateway, this)
{
   // empty
}

TreeServerSideSession :: ~TreeServerSideSession()
{
   // empty
}

void TreeServerSideSession :: MessageReceivedFromGateway(const MessageRef & msg, void * userData)
{
   if (IncomingTreeMessageReceivedFromClient(msg) == B_UNIMPLEMENTED) StorageReflectSession::MessageReceivedFromGateway(msg, userData);
}

TreeServerSideSessionFactory :: TreeServerSideSessionFactory(ITreeGateway * upstreamGateway)
   : ITreeGatewaySubscriber(upstreamGateway)
{
   // empty
}

AbstractReflectSessionRef TreeServerSideSessionFactory :: CreateSession(const String & /*clientAddress*/, const IPAddressAndPort & /*factoryInfo*/)
{
   TreeServerSideSessionRef ret(newnothrow TreeServerSideSession(GetGateway()));
   if (ret() == NULL) WARN_OUT_OF_MEMORY;
   return ret;
}

};

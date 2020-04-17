#include "zg/gateway/tree/TreeClientSideSession.h"

namespace zg {

TreeClientSideSession :: TreeClientSideSession()
   : NetworkTreeGatewaySubscriber(NULL, this)
   , _networkGateway(NULL, this)
   , _muxGateway(&_networkGateway)
{
   NetworkTreeGatewaySubscriber::SetGateway(&_networkGateway);  // gotta do this here, *after* _networkGateway is constructed
}

TreeClientSideSession :: ~TreeClientSideSession()
{
   _muxGateway.ShutdownGateway();
}

void TreeClientSideSession :: AsyncConnectCompleted()
{
   AbstractReflectSession::AsyncConnectCompleted();
   _networkGateway.SetNetworkConnected(true);
}

bool TreeClientSideSession :: ClientConnectionClosed()
{
   const bool ret = AbstractReflectSession::ClientConnectionClosed();
   _networkGateway.SetNetworkConnected(false);
   return ret; 
}

void TreeClientSideSession :: MessageReceivedFromGateway(const MessageRef & msg, void * userData)
{
   (void) IncomingTreeMessageReceivedFromServer(msg);
}

};

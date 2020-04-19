#include "zg/gateway/tree/TreeClientSideSession.h"

namespace zg {

TreeClientSideSession :: TreeClientSideSession()
   : MuxTreeGateway(NULL)  // gotta pass NULL here since _networkGateway hasn't been constructed yet
   , _networkGateway(this)
{
   MuxTreeGateway::SetGateway(&_networkGateway);  // gotta do this here, *after* _networkGateway is constructed
}

TreeClientSideSession :: ~TreeClientSideSession()
{
   // empty
}

void TreeClientSideSession :: AboutToDetachFromServer()
{
   ShutdownGateway();
   AbstractReflectSession::AboutToDetachFromServer();
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
   _networkGateway.IncomingTreeMessageReceivedFromServer(msg);
}

};

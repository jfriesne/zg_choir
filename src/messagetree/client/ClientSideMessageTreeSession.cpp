#include "zg/messagetree/client/ClientSideMessageTreeSession.h"

namespace zg {

ClientSideMessageTreeSession :: ClientSideMessageTreeSession()
   : MuxTreeGateway(NULL)  // gotta pass NULL here since _networkGateway hasn't been constructed yet
   , _networkGateway(this)
{
   MuxTreeGateway::SetGateway(&_networkGateway);  // gotta do this here, *after* _networkGateway is constructed
}

ClientSideMessageTreeSession :: ~ClientSideMessageTreeSession()
{
   // empty
}

void ClientSideMessageTreeSession :: AboutToDetachFromServer()
{
   ShutdownGateway();
   AbstractReflectSession::AboutToDetachFromServer();
}

void ClientSideMessageTreeSession :: AsyncConnectCompleted()
{
   AbstractReflectSession::AsyncConnectCompleted();
   _networkGateway.SetNetworkConnected(true);
}

bool ClientSideMessageTreeSession :: ClientConnectionClosed()
{
   const bool ret = AbstractReflectSession::ClientConnectionClosed();
   _networkGateway.SetNetworkConnected(false);
   return ret;
}

void ClientSideMessageTreeSession :: MessageReceivedFromGateway(const MessageRef & msg, void * /*userData*/)
{
   _networkGateway.IncomingTreeMessageReceivedFromServer(msg);
}

};

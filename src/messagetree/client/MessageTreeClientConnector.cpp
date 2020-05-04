#include "zg/messagetree/client/MessageTreeClientConnector.h"

namespace zg {

MessageTreeClientConnector :: MessageTreeClientConnector(ICallbackMechanism * mechanism, const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalCriteria)
   : ClientConnector(mechanism, signaturePattern, systemNamePattern, optAdditionalCriteria)
   , MuxTreeGateway(NULL)  // gotta pass NULL here since _networkGateway hasn't been constructed yet
   , _networkGateway(this)
{
   MuxTreeGateway::SetGateway(&_networkGateway);  // gotta do this here, *after* _networkGateway is constructed
}

MessageTreeClientConnector :: ~MessageTreeClientConnector()
{
   Stop();
   ShutdownGateway();
}

void MessageTreeClientConnector :: ConnectionStatusUpdated(const MessageRef & optServerInfo)
{
   _networkGateway.SetNetworkConnected(optServerInfo() != NULL);
}

void MessageTreeClientConnector :: MessageReceivedFromNetwork(const MessageRef & msg)
{
   _networkGateway.IncomingTreeMessageReceivedFromServer(msg);
}

};

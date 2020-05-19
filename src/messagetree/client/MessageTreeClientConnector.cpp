#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/TreeConstants.h"

namespace zg {

MessageTreeClientConnector :: MessageTreeClientConnector(ICallbackMechanism * mechanism, const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalCriteria)
   : ClientConnector(mechanism, signaturePattern, systemNamePattern, optAdditionalCriteria)
   , MuxTreeGateway(NULL)  // gotta pass NULL here since _networkGateway hasn't been constructed yet
   , _networkGateway(this)
   , _undoKey(String("uk%1").Arg(GetCurrentTime64() + GetRunTime64() + ((uintptr)this) + ((uint64)rand()) + (((uint64)rand())<<32)))
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
   if (optServerInfo())
   {
      status_t ret;
      MessageRef setUndoKeyMsg = GetMessageFromPool(TREE_COMMAND_SETUNDOKEY);
      if ((setUndoKeyMsg() == NULL)||(setUndoKeyMsg()->CAddString(TREE_NAME_UNDOKEY, _undoKey).IsError(ret))||(SendOutgoingMessageToNetwork(setUndoKeyMsg).IsError(ret)))
         LogTime(MUSCLE_LOG_ERROR, "MessageTreeClientConnector %p:  Error setting undo-key [%s]\n", this, ret());
   }
   _networkGateway.SetNetworkConnected(optServerInfo() != NULL);
}

void MessageTreeClientConnector :: MessageReceivedFromNetwork(const MessageRef & msg)
{
   _networkGateway.IncomingTreeMessageReceivedFromServer(msg);
}

};

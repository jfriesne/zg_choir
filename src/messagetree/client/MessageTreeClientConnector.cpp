#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/TreeConstants.h"
#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_GETPARAMETERS and PR_RESULT_PARAMETERS

namespace zg {

MessageTreeClientConnector :: MessageTreeClientConnector(ICallbackMechanism * mechanism)
   : ClientConnector(mechanism)
   , MuxTreeGateway(NULL)  // gotta pass NULL here since _networkGateway hasn't been constructed yet
   , _networkGateway(this)
   , _expectingParameters(false)
{
   unsigned seed = time(NULL);
   _undoKey = String("uk%1").Arg(GetCurrentTime64() + GetRunTime64() + ((uintptr)this) + ((uint64)rand_r(&seed)) + (((uint64)rand_r(&seed))<<32));

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
      if (SendOutgoingMessageToNetwork(GetMessageFromPool(PR_COMMAND_GETPARAMETERS)).IsOK(ret)) _expectingParameters = true;
      else
      {
         LogTime(MUSCLE_LOG_ERROR, "Couldn't send PR_COMMAND_GETPARAMETERS to server! [%s]\n", ret());
         ret = B_NO_ERROR;  // clear the error code
      }

      MessageRef setUndoKeyMsg = GetMessageFromPool(TREE_COMMAND_SETUNDOKEY);
      if ((setUndoKeyMsg() == NULL)||(setUndoKeyMsg()->CAddString(TREE_NAME_UNDOKEY, _undoKey).IsError(ret))||(SendOutgoingMessageToNetwork(setUndoKeyMsg).IsError(ret)))
         LogTime(MUSCLE_LOG_ERROR, "MessageTreeClientConnector %p:  Error setting undo-key [%s]\n", this, ret());

      // We'll call SetNetworkConnected(true) only after we get the PR_RESULT_PARAMETERS back
      // that way there won't be a short period where the ITreeGatewaySubscribers think everything
      // is copacetic but we don't have the parameter-info available yet
   }
   else _networkGateway.SetNetworkConnected(false);
}

void MessageTreeClientConnector :: MessageReceivedFromNetwork(const MessageRef & msg)
{
   if ((_expectingParameters)&&(msg()->what == PR_RESULT_PARAMETERS))
   {
      _expectingParameters = false;
      _networkGateway.SetParameters(msg);
      _networkGateway.SetNetworkConnected(true);
   }
   else _networkGateway.IncomingTreeMessageReceivedFromServer(msg);
}

};

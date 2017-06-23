#include "iogateway/SignalMessageIOGateway.h"

#include "zg/private/PZGThreadedSession.h"

namespace zg_private
{

PZGThreadedSession :: PZGThreadedSession()
{
   // empty
}

ConstSocketRef PZGThreadedSession :: CreateDefaultSocket()
{
   return GetOwnerWakeupSocket();
}

AbstractMessageIOGatewayRef PZGThreadedSession :: CreateGateway()
{
   SignalMessageIOGatewayRef gwRef(newnothrow SignalMessageIOGateway);
   if (gwRef() == NULL) WARN_OUT_OF_MEMORY;
   return gwRef;
}

status_t PZGThreadedSession :: AttachedToServer()
{
   if (AbstractReflectSession::AttachedToServer() != B_NO_ERROR) return B_ERROR;
   if (GetOwnerWakeupSocket() == NULL) return B_ERROR;  // paranoia?
   return StartInternalThread();
}

void PZGThreadedSession :: AboutToDetachFromServer()
{
   ShutdownInternalThread();
   AbstractReflectSession::AboutToDetachFromServer();
}

void PZGThreadedSession :: EndSession()
{
   ShutdownInternalThread();   // avoid possible race conditions in subclasses
   AbstractReflectSession::EndSession();
}

void PZGThreadedSession :: MessageReceivedFromGateway(const MessageRef & /*msg*/, void * /*userData*/)
{
   MessageRef msgFromThread;
   while(GetNextReplyFromInternalThread(msgFromThread) >= 0) MessageReceivedFromInternalThread(msgFromThread);
}

status_t PZGThreadedSession :: TellInternalThreadToRecreateMulticastSockets()
{
   static Message recreateSocketsMsg(PZG_THREADED_SESSION_RECREATE_SOCKETS);
   MessageRef msgRef(&recreateSocketsMsg, false);

   return SendMessageToInternalThread(msgRef);
}

};  // end namespace zg_private

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
   if (gwRef() == NULL) MWARN_OUT_OF_MEMORY;
   return gwRef;
}

status_t PZGThreadedSession :: AttachedToServer()
{
   status_t ret;
   if (AbstractReflectSession::AttachedToServer().IsError(ret)) return ret;

   if (GetOwnerWakeupSocket()() == NULL) return B_BAD_OBJECT;  // paranoia?
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
   int32 numLeft;
   while((numLeft = GetNextReplyFromInternalThread(msgFromThread)) >= 0) MessageReceivedFromInternalThread(msgFromThread, numLeft);
}

status_t PZGThreadedSession :: TellInternalThreadToRecreateMulticastSockets()
{
   static Message _recreateSocketsMsg(PZG_THREADED_SESSION_RECREATE_SOCKETS);
   return SendMessageToInternalThread(DummyMessageRef(_recreateSocketsMsg));
}

};  // end namespace zg_private

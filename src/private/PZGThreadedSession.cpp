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
   return AbstractMessageIOGatewayRef(new SignalMessageIOGateway);
}

status_t PZGThreadedSession :: AttachedToServer()
{
   MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());

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
   uint32 numLeft = 0;
   while(GetNextReplyFromInternalThread(msgFromThread, 0, &numLeft).IsOK()) MessageReceivedFromInternalThread(msgFromThread, numLeft);
}

status_t PZGThreadedSession :: TellInternalThreadToRecreateMulticastSockets()
{
   static Message _recreateSocketsMsg(PZG_THREADED_SESSION_RECREATE_SOCKETS);
   return SendMessageToInternalThread(DummyMessageRef(_recreateSocketsMsg));
}

};  // end namespace zg_private

#include "glue/SocketCallbackMechanism.h"
#include "util/NetworkUtilityFunctions.h"

namespace zg {

SocketCallbackMechanism :: SocketCallbackMechanism()
{
   status_t ret;
   if (CreateConnectedSocketPair(_dispatchThreadSock, _otherThreadsSock, false).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "SocketCallbackMechanism:  Unable to create notify-socket-pair! [%s]\n", ret());
   }
}

SocketCallbackMechanism :: ~SocketCallbackMechanism()
{
   // empty
}

void SocketCallbackMechanism :: DispatchCallbacks()
{
   // read and discard any signalling-bytes sent by the other threads
   char junkBuf[128];
   (void) ReceiveData(_dispatchThreadSock, junkBuf, sizeof(junkBuf), false);

   // Call superclass to perform the actual callbacks
   ICallbackMechanism::DispatchCallbacks();
}

void SocketCallbackMechanism :: SignalDispatchThread()
{
   const char junk = 'j';
   if (SendData(_otherThreadsSock, &junk, sizeof(junk), false) != sizeof(junk)) 
   {
      LogTime(MUSCLE_LOG_ERROR, "SocketCallbackMechanism::SignalDispatchThread():  Unable to send notification byte! [%s]\n", B_ERRNO());
   }
}

};  // end zg namespace

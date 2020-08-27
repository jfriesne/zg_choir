#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "system/SetupSystem.h"
#include "util/SocketMultiplexer.h"
#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/client/TestTreeGatewaySubscriber.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "zg/callback/SocketCallbackMechanism.h"

using namespace zg;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   const String systemName = (argc >= 2) ? argv[1] : "test_tree_system";
   LogTime(MUSCLE_LOG_INFO, "Connecting to a server in tree_server system named [%s]...\n", systemName());

   SocketCallbackMechanism scm; // will call our callback methods as appropriate

   StdinDataIO stdinDataIO(false);  // will listen for text input on stdin
   PlainTextMessageIOGateway stdinGateway;  // will parse the text input from stdin into lines for us
   stdinGateway.SetDataIO(DataIORef(&stdinDataIO, false));

   status_t ret;
   MessageTreeClientConnector mtcc(&scm);  // handles our TCP connection to a server
   if (mtcc.Start("tree_server", systemName).IsOK(ret))
   {
      TestTreeGatewaySubscriber testTreeGatewaySubscriber(&mtcc);

      bool keepGoing = true;
      SocketMultiplexer sm;
      while(keepGoing)
      {
         (void) sm.RegisterSocketForReadReady(scm.GetDispatchThreadNotifierSocket().GetFileDescriptor());
         (void) sm.RegisterSocketForReadReady(stdinDataIO.GetReadSelectSocket().GetFileDescriptor());
         if (sm.WaitForEvents() >= 0) 
         {
            // Handle dispatch-callback-signals from our SocketCallbackMechanism
            if (sm.IsSocketReadyForRead(scm.GetDispatchThreadNotifierSocket().GetFileDescriptor())) scm.DispatchCallbacks();

            // Handle input text from the local user on stdin
            if (sm.IsSocketReadyForRead(stdinDataIO.GetReadSelectSocket().GetFileDescriptor()))
            {
               QueueGatewayMessageReceiver stdinQ;
               while(1)
               {
                  const int32 numStdinBytesRead = stdinGateway.DoInput(stdinQ);
                  if (numStdinBytesRead < 0)
                  {
                     LogTime(MUSCLE_LOG_CRITICALERROR, "Stdin closed, exiting!\n");
                     keepGoing = false;
                     break;
                  }
                  else
                  {
                     MessageRef nextMsg;
                     while(stdinQ.RemoveHead(nextMsg).IsOK())
                     {
                        const String * nextLine;
                        for (int32 i=0; nextMsg()->FindString(PR_NAME_TEXT_LINE, i, &nextLine).IsOK(); i++) testTreeGatewaySubscriber.TextCommandReceived(*nextLine);
                     }

                     if (numStdinBytesRead == 0) break;
                  }
               }
            } 
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed, exiting! [%s]\n", B_ERROR());
            break; 
         }
      }
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Error, couldn't start connector thread, exiting! [%s]\n", ret());

   return 0;
}

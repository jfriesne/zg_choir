#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"

#include "zg/ZGStdinSession.h"
#include "zg/messagetree/client/ClientSideMessageTreeSession.h"
#include "zg/messagetree/client/TestTreeGatewaySubscriber.h"

using namespace zg;

// This class reads users input from stdin and works as a test-bed for our ITreeGatewaySubscriber functionality
class TreeClientStdinSession : public ZGStdinSession, public TestTreeGatewaySubscriber
{
public:
   TreeClientStdinSession(ITreeGateway * gateway) : ZGStdinSession(*this, true), TestTreeGatewaySubscriber(gateway)
   {
      // empty
   }

   virtual void TreeGatewayConnectionStateChanged()
   {
      TestTreeGatewaySubscriber::TreeGatewayConnectionStateChanged();

      const bool isConnected = IsTreeGatewayConnected();
      if (isConnected == false)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Connection to server lost, exiting!\n");
         EndServer();  // tell our local ReflectServer::ServerProcessLoop() call to return so that this client process can exit cleanly
      }
   }
};

int main(int argc, char ** argv)
{
   const uint16 TREE_PEER_SERVER_PORT = 4444;

   int exitCode = 10;

   // This object is required by the MUSCLE library;
   // it does various system-specific startup and shutdown tasks
   CompleteSetupSystem css;

   // Handling of various optional command-line arguments
   Message args; (void) ParseArgs(argc, argv, args);
   (void) args.AddString("debugcrashes", "");  // let's make sure to print a stack trace if we crash
   HandleStandardDaemonArgs(args);

   String host;
   uint16 port;
   if (ParseConnectArg(args, "host", host, port).IsError())
   {
      LogTime(MUSCLE_LOG_WARNING, "No host=hostname:port argument specified; defaulting to 127.0.0.1:%u\n", TREE_PEER_SERVER_PORT);
      host = "127.0.0.1";
      port = TREE_PEER_SERVER_PORT;
   }
   if (port == 0) port = TREE_PEER_SERVER_PORT;

   // This object will connect to the tree_server process
   ClientSideMessageTreeSession clientSession;

   // This object will read from stdin for us, so we can accept typed text commands from the user
   TreeClientStdinSession stdinSession(&clientSession);

   // This object implements the standard MUSCLE event loop
   ReflectServer server;

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   status_t ret;
   if ((server.AddNewSession(DummyZGStdinSessionRef(stdinSession)).IsOK(ret))&&(server.AddNewConnectSession(DummyAbstractReflectSessionRef(clientSession), IPAddressAndPort(GetHostByName(host()), port)).IsOK(ret)))
   {
      // Virtually all of the program's execution time happens inside the ServerProcessLoop() method

      LogTime(MUSCLE_LOG_INFO, "tree_client is accepting commands on stdin.  Enter '?' for a list of available commands.\n");
      ret = server.ServerProcessLoop();  // doesn't return until it's time to exit
      if (ret.IsOK())
      {
         LogTime(MUSCLE_LOG_INFO, "Event loop exited normally.\n");
         exitCode = 0;
      }
      else LogTime(MUSCLE_LOG_ERROR, "Event loop aborted!\n");
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set up sessions [%s]!\n", ret());

   // Required in order to ensure an orderly shutdown
   server.Cleanup();

   return exitCode;
}


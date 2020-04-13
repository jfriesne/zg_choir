#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"

#include "zg/ZGStdinSession.h"
#include "zg/gateway/tree/TreeClientSideSession.h"

using namespace zg;

// This class reads users input from stdin and works as a test-bed for our ITreeGatewaySubscriber functionality
class TreeClientStdinSession : public ZGStdinSession, public ITextCommandReceiver, public ITreeGatewaySubscriber
{
public:
   TreeClientStdinSession(ITreeGateway * gateway) : ZGStdinSession(*this, true), ITreeGatewaySubscriber(gateway)
   {
      // empty
   }

   virtual bool IsReadyForTextCommands() const {return IsTreeGatewayConnected();}

   virtual bool TextCommandReceived(const String & cmd)
   {
      printf("TreeClientStdinSession::TextCommandReceived(%s)\n", cmd());
      return true;
   }

   virtual void TreeCallbackBatchBeginning()
   {
      printf("TreeClientStdinSession::TreeCallbackBatchBeginning)\n");
   }

   virtual void TreeCallbackBatchEnding()
   {
      printf("TreeClientStdinSession::TreeCallbackBatchEnding)\n");
   }

   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg)
   {
      printf("TreeClientStdinSession::TreeNodeUpdated(%s,%p)\n", nodePath(), payloadMsg());
      if (payloadMsg()) payloadMsg()->PrintToStream();
   }

   virtual void TreeNodeIndexCleared(const String & path)
   {
      printf("TreeClientStdinSession::TreeNodeIndexCleared(%s)\n", path());
   }

   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName)
   {
      printf("TreeClientStdinSession::TreeNodeIndexEntryInserted(%s," UINT32_FORMAT_SPEC ",%s)\n", path(), insertedAtIndex, nodeName());
   }

   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName)
   {
      printf("TreeClientStdinSession::TreeNodeIndexEntryRemoved(%s," UINT32_FORMAT_SPEC ",%s)\n", path(), removedAtIndex, nodeName());
   }

   virtual void TreeServerPonged(const String & tag)
   {
      printf("TreeClientStdinSession::TreeServerPonged(%s)\n", tag());
   }

   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
   {
      printf("TreeClientStdinSession::SubtreesRequestResultReturned(%s,%p)\n", tag(), subtreeData());
      if (subtreeData()) subtreeData()->PrintToStream();
   }

   virtual void TreeGatewayConnectionStateChanged()
   {
      printf("TreeClientStdinSession::TextCommandReceived(%i)\n", IsTreeGatewayConnected());
   }

   virtual void TreeGatewayShuttingDown()
   {
      printf("TreeClientStdinSession::TreeGatewayShuttingDown()\n");
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
   args.AddString("debugcrashes", "");  // let's make sure to print a stack trace if we crash
   HandleStandardDaemonArgs(args);

   String host;
   uint16 port;
   if (ParseConnectArg(args, "host", host, port).IsError())
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Usage:  ./tree_client host=127.0.0.1:%u", TREE_PEER_SERVER_PORT);
      return 10;
   }
   if (port == 0) port = TREE_PEER_SERVER_PORT;

   // This object will connect to the tree_server process
   TreeClientSideSession clientSession;

   // This object will read from stdin for us, so we can accept typed text commands from the user
   TreeClientStdinSession stdinSession(clientSession.GetTreeGateway());

   // This object implements the standard MUSCLE event loop
   ReflectServer server;

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   status_t ret;
   if ((server.AddNewSession(ZGStdinSessionRef(&stdinSession, false)).IsOK(ret))&&(server.AddNewConnectSession(AbstractReflectSessionRef(&clientSession, false), host, port).IsOK(ret)))
   {
      // Virtually all of the program's execution time happens inside the ServerProcessLoop() method
      ret = server.ServerProcessLoop();  // doesn't return until it's time to exit
      if (ret == B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_INFO, "Event loop exited normally.\n");
         exitCode = 0;
      }
      else LogTime(MUSCLE_LOG_ERROR, "Event loop aborted!\n");

      // Required in order to ensure an orderly shutdown
      server.Cleanup();
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set up sessions [%s]!\n", ret());

   return exitCode;
}


#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"

#include "zg/ZGStdinSession.h"
#include "zg/messagetree/client/ClientSideMessageTreeSession.h"

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

   virtual bool TextCommandReceived(const String & textStr)
   {
      LogTime(MUSCLE_LOG_INFO, "You typed: [%s]\n", textStr());

      StringTokenizer tok(textStr(), " ");
      const char * cmd = tok();
      if (cmd == NULL) cmd = "";

      status_t ret;
      switch(cmd[0])
      {
#ifdef REDO_THIS
         case 'm':
            ref()->what = MAKETYPE("umsg");
            if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
            ref()->AddString("info", "This is a user message");
         break;
#endif

         case 'p': case 'P':
         {
            const bool isSeniorPing = (cmd[0] == 'P');
            const String pingTag    = tok();
            if (PingTreeServer(pingTag, isSeniorPing?TreeGatewayFlags(TREE_GATEWAY_FLAG_TOSENIOR):TreeGatewayFlags()).IsOK(ret)) 
            {
               LogTime(MUSCLE_LOG_INFO, "Sent %s ping with tag [%s]\n", isSeniorPing?"senior":"normal", pingTag());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error, couldn't send %s ping with tag [%s] (%s)\n", isSeniorPing?"senior":"normal", pingTag(), ret()); 
         }
         break;
 
         case 's':
         {
            const String path = tok();
          
            MessageRef payloadMsg = GetMessageFromPool(1234);
            payloadMsg()->AddString("This node was posted at: ", GetHumanReadableTimeString(GetRunTime64()));
            if (UploadTreeNodeValue(path, payloadMsg).IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Uploaded Message to relative path [%s]\n", path());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error uploading Message to relative path [%s] (%s)\n", path(), ret());
         }
         break;

         case 'd':
         {
            const String path = tok();
          
            if (RequestDeleteTreeNodes(path).IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Requested deletion of node(s) matching [%s]\n", path());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error requesting deletion of nodes matching path [%s] (%s)\n", path(), ret());
         }
         break;

         case 'g':
         {
            const String path = tok();
          
            if (RequestTreeNodeValues(path).IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Requested download of node(s) matching [%s]\n", path());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error requesting download of nodes matching path [%s] (%s)\n", path(), ret());
         }
         break;

         case 'G':
         {
            const String path = tok();
            const String maxDepthStr = tok();
            const String tag = tok();
          
            const uint32 maxDepth = ((maxDepthStr.HasChars())&&(muscleInRange(maxDepthStr[0], '0', '9'))) ? atol(maxDepthStr()) : MUSCLE_NO_LIMIT;

            Queue<String> paths; (void) paths.AddTail(path);
            if (RequestTreeNodeSubtrees(paths, Queue<ConstQueryFilterRef>(), tag, maxDepth).IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Requested download of subtrees(s) matching [%s]\n", path());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error requesting download of subtrees matching path [%s] (%s)\n", path(), ret());
         }
         break;

         case 'S':
         {
            const String path = tok();
          
            if (AddTreeSubscription(path).IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Subscribed to node(s) matching [%s]\n", path());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error subscribing to nodes matching path [%s] (%s)\n", path(), ret());
         }
         break;

         case 'U':
         {
            const String path = tok();
          
            if (RemoveTreeSubscription(path).IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Unsubscribed from node(s) matching [%s]\n", path());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error unsubscribing from nodes matching path [%s] (%s)\n", path(), ret());
         }
         break;

         case 'Z':
         {
            if (RemoveAllTreeSubscriptions().IsOK(ret))
            {
               LogTime(MUSCLE_LOG_INFO, "Unsubscribed all node subscriptions\n");
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error unsubscribing from all node subscriptions (%s)\n", ret());
         }
         break;

         default:
            LogTime(MUSCLE_LOG_ERROR, "Sorry, wot?\n");
            return false;
      }
      return true;
   }

   virtual void TreeCallbackBatchBeginning()
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeCallbackBatchBeginning)\n");
   }

   virtual void TreeCallbackBatchEnding()
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeCallbackBatchEnding)\n");
   }

   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg)
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeUpdated(%s,%p)\n", nodePath(), payloadMsg());
      if (payloadMsg()) payloadMsg()->PrintToStream();
   }

   virtual void TreeNodeIndexCleared(const String & path)
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeIndexCleared(%s)\n", path());
   }

   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName)
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeIndexEntryInserted(%s," UINT32_FORMAT_SPEC ",%s)\n", path(), insertedAtIndex, nodeName());
   }

   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName)
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeIndexEntryRemoved(%s," UINT32_FORMAT_SPEC ",%s)\n", path(), removedAtIndex, nodeName());
   }

   virtual void TreeServerPonged(const String & tag)
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeServerPonged(%s)\n", tag());
   }

   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::SubtreesRequestResultReturned(%s,%p)\n", tag(), subtreeData());
      if (subtreeData()) subtreeData()->PrintToStream();
   }

   virtual void TreeGatewayConnectionStateChanged()
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TextCommandReceived(%i)\n", IsTreeGatewayConnected());
   }

   virtual void TreeGatewayShuttingDown()
   {
      LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeGatewayShuttingDown()\n");
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
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set up sessions [%s]!\n", ret());

   // Required in order to ensure an orderly shutdown
   server.Cleanup();

   return exitCode;
}


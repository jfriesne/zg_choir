#include "zg/messagetree/client/TestTreeGatewaySubscriber.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)
#include "util/StringTokenizer.h"

namespace zg {

TestTreeGatewaySubscriber :: TestTreeGatewaySubscriber(ITreeGateway * gateway) : ITreeGatewaySubscriber(gateway), _opTagCounter(0)
{
   /* empty */
}

TestTreeGatewaySubscriber :: ~TestTreeGatewaySubscriber()
{
   /* empty */
}

bool TestTreeGatewaySubscriber :: IsReadyForTextCommands() const 
{
   return IsTreeGatewayConnected();
}

bool TestTreeGatewaySubscriber :: TextCommandReceived(const String & textStr)
{
   if (textStr.IsEmpty()) return false;

   LogTime(MUSCLE_LOG_INFO, "You typed: [%s]\n", textStr());

   StringTokenizer tok(textStr(), NULL);
   const char * cmd = tok();
   if (cmd == NULL) cmd = "";

   String optOpTag;

   status_t ret;
   switch(cmd[0])
   {
      case '?':
      {
         LogTime(MUSCLE_LOG_INFO, "tree_client command set is as follows:\n");
         LogTime(MUSCLE_LOG_INFO, "  p <tag>       -- Ping the client's local server with the given tag-string\n"); 
         LogTime(MUSCLE_LOG_INFO, "  P <tag>       -- Ping the senior peer with the given tag-string\n"); 
         LogTime(MUSCLE_LOG_INFO, "  s dbs/db_0/x  -- set a DataNode at the given path\n");
         LogTime(MUSCLE_LOG_INFO, "  d dbs/db_0/*  -- delete one or more nodes or node-subtrees\n");
         LogTime(MUSCLE_LOG_INFO, "  g dbs/db_*/*  -- submit a one-time query for the current state of nodes matching this path\n");
         LogTime(MUSCLE_LOG_INFO, "  G dbs/db_0    -- submit a one-time query for the node-subtree at the given path\n");
         LogTime(MUSCLE_LOG_INFO, "  i dbs/ [I5]   -- insert an indexed-node under the given parent node (optionally provide name of node to insert before)\n");
         LogTime(MUSCLE_LOG_INFO, "  m dbs/I5 [I2] -- move an indexed-node to a new position within its parent-node's index-list\n");
         LogTime(MUSCLE_LOG_INFO, "  S dbs/db_*/*  -- subscribe to nodes matching this path\n");
         LogTime(MUSCLE_LOG_INFO, "  U dbs/**/*    -- unsubscribe from nodes matching this path\n");
         LogTime(MUSCLE_LOG_INFO, "  Z             -- unsubscribe from all this client's subscriptions\n");
         LogTime(MUSCLE_LOG_INFO, "  ?             -- print this text\n");
      }
      break;

      case 'p':
      {
         const String pingTag = tok();
         if (PingTreeLocalPeer(pingTag).IsOK(ret)) 
         {
            LogTime(MUSCLE_LOG_INFO, "Sent server-ping with tag [%s]\n", pingTag());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error, couldn't send server-ping with tag [%s] (%s)\n", pingTag(), ret()); 
      }
      break;

      case 'P':
      {
         const String pingTag = tok();
         if (PingTreeSeniorPeer(pingTag).IsOK(ret))   // assuming we want the ping to route through the update-path of database #0, for now 
         {
            LogTime(MUSCLE_LOG_INFO, "Sent senior-peer-ping with tag [%s]\n", pingTag());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error, couldn't send senior-peer-ping with tag [%s] (%s)\n", pingTag(), ret()); 
      }
      break;

      case 's':
      {
         const String path = tok();
       
         MessageRef payloadMsg = GetMessageFromPool(1234);
         payloadMsg()->AddString("This node was posted at: ", GetHumanReadableTimeString(GetRunTime64()));
         if (UploadTreeNodeValue(path, payloadMsg, TreeGatewayFlags(), GetEmptyString(), GenerateOpTag(optOpTag)).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Uploaded Message to relative path [%s] [%s]\n", path(), optOpTag());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error uploading Message to relative path [%s] (%s)\n", path(), ret());
      }
      break;

      case 'i':
      {
         const String path = tok();
       
         MessageRef payloadMsg = GetMessageFromPool(1234);
         payloadMsg()->AddString("This indexed node was posted at: ", GetHumanReadableTimeString(GetRunTime64()));

         const String optBefore = tok();

         if (UploadTreeNodeValue(path, payloadMsg, TreeGatewayFlags(TREE_GATEWAY_FLAG_INDEXED), optBefore).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Uploaded indexed Message to relative path [%s] (before [%s])\n", path(), optBefore.HasChars()?optBefore():NULL);
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error uploading indexed Message to relative path [%s] (before [%s]) (%s)\n", path(), optBefore.HasChars()?optBefore():NULL, ret());
      }
      break;

      case 'm':
      {
         const String path      = tok();
         const String optBefore = tok();

         if (RequestMoveTreeIndexEntry(path, optBefore, ConstQueryFilterRef(), TreeGatewayFlags(), GenerateOpTag(optOpTag)).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Moved indexed-node [%s] to before [%s] [%s]\n", path(), optBefore.HasChars()?optBefore():NULL, optOpTag());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error moving indexed-node [%s] to before [%s] (%s)\n", path(), optBefore.HasChars()?optBefore():NULL, ret());
      }
      break;

      case 'd':
      {
         const String path = tok();
       
         if (RequestDeleteTreeNodes(path, ConstQueryFilterRef(), TreeGatewayFlags(), GenerateOpTag(optOpTag)).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Requested deletion of node(s) matching [%s] [%s]\n", path(), optOpTag());
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
         const String path        = tok();
         const String tag         = tok();
         const String maxDepthStr = tok();
       
         const uint32 maxDepth = ((maxDepthStr.HasChars())&&(muscleInRange(maxDepthStr[0], '0', '9'))) ? atol(maxDepthStr()) : MUSCLE_NO_LIMIT;

         Queue<String> paths; (void) paths.AddTail(path);
         if (RequestTreeNodeSubtrees(paths, Queue<ConstQueryFilterRef>(), tag, maxDepth).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Requested download of subtrees(s) matching [%s], using tag [%s] and maxDepth=" UINT32_FORMAT_SPEC "\n", path(), tag(), maxDepth);
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error requesting download of subtrees matching path [%s] using tag [%s] (%s)\n", path(), tag(), ret());
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
         if (*cmd) LogTime(MUSCLE_LOG_ERROR, "Sorry, wot?\n");
         return false;
   }
   return true;
}

String TestTreeGatewaySubscriber :: GenerateOpTag(String & retTag)
{
   char buf[128];
   muscleSprintf(buf, "optag-%i", ++_opTagCounter);
   retTag = buf;
   return retTag;
}

void TestTreeGatewaySubscriber :: CallbackBatchBegins()
{
   IGatewaySubscriber::CallbackBatchBegins();
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::CallbackBatchBegins()\n");
}

void TestTreeGatewaySubscriber :: CallbackBatchEnds()
{
   IGatewaySubscriber::CallbackBatchEnds();
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::CallbackBatchEnds()\n");
}

void TestTreeGatewaySubscriber :: TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg, const String & optOpTag)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeUpdated(%s,%p) optOpTag=[%s]\n", nodePath(), payloadMsg(), optOpTag());
   if (payloadMsg()) payloadMsg()->PrintToStream();
}

void TestTreeGatewaySubscriber :: TreeNodeIndexCleared(const String & path, const String & optOpTag)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeIndexCleared(%s) optOpTag=[%s]\n", path(), optOpTag());
}

void TestTreeGatewaySubscriber :: TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeIndexEntryInserted(%s," UINT32_FORMAT_SPEC ",%s) optOpTag=[%s]\n", path(), insertedAtIndex, nodeName(), optOpTag());
}

void TestTreeGatewaySubscriber :: TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeNodeIndexEntryRemoved(%s," UINT32_FORMAT_SPEC ",%s) optOpTag=[%s]\n", path(), removedAtIndex, nodeName(), optOpTag());
}

void TestTreeGatewaySubscriber :: TreeLocalPeerPonged(const String & tag)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeLocalPeerPonged(%s)\n", tag());
}

void TestTreeGatewaySubscriber :: TreeSeniorPeerPonged(const String & tag, uint32 whichDB)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeSeniorPeerPonged(" UINT32_FORMAT_SPEC ", %s)\n", whichDB, tag());
}

void TestTreeGatewaySubscriber :: MessageReceivedFromTreeSeniorPeer(int32 whichDB, const String & tag, const MessageRef & payload)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::MessageReceivedFromTreeSeniorPeer(" UINT32_FORMAT_SPEC ", %s)\n", whichDB, tag());
   payload()->PrintToStream();
}

void TestTreeGatewaySubscriber :: MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::MessageReceivedFromSubscriber(nodePath=[%s] returnAddress=[%s])\n", nodePath(), returnAddress());
   payload()->PrintToStream();
}

void TestTreeGatewaySubscriber :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::SubtreesRequestResultReturned(%s,%p)\n", tag(), subtreeData());
   if (subtreeData()) subtreeData()->PrintToStream();
}

void TestTreeGatewaySubscriber :: TreeGatewayConnectionStateChanged()
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeGatewayConnectionStateChanged(%s)\n", IsTreeGatewayConnected()?"to CONNECTED":"to DISCONNECTED");
}

void TestTreeGatewaySubscriber :: TreeGatewayShuttingDown()
{
   LogTime(MUSCLE_LOG_INFO, "TreeClientStdinSession::TreeGatewayShuttingDown()\n");
}

};

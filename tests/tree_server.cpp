#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"

#include "zg/ZGStdinSession.h"
#include "zg/discovery/server/DiscoveryServerSession.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"

using namespace zg;

enum {
   TREE_DB_COMMAND_SET_DB_STATE = 1953654117, // 'tree' -- clears the current DB state and then adds the specified key/value pairs
   TREE_DB_COMMAND_PUT_STRINGS,               //        -- adds the specified key/value pairs (overwriting the value of any existing keys that match the new keys)
   TREE_DB_COMMAND_REMOVE_STRINGS,            //        -- removes any key/value pairs whose keys match those found in the Message
   TREE_DB_COMMAND_USER_TEXT,                 //        -- just some chat text to print when received, for testing
};

// Some example-databases we might want to manage separately from each other
enum {
   TREE_DATABASE_DEFAULT = 0,   // default database for all nodes that aren't in a more well-defined db-category
   TREE_DATABASE_SERVERINFO,    // system's information about what servers are currently online
   TREE_DATABASE_CLIENTINFO,    // system's information about what clients are currently online
   TREE_DATABASE_LOG,           // system log messages go here
   NUM_TREE_DATABASES
};

// Returns the session-relative sub-path of the sub-tree that each ZG database should control
static String GetDatabaseRootPath(uint32 whichDB)
{
   switch(whichDB)
   {
      case TREE_DATABASE_DEFAULT:    return GetEmptyString();   // umbrella DB to handle anything the other ones don't
      case TREE_DATABASE_SERVERINFO: return "srv";
      case TREE_DATABASE_CLIENTINFO: return "cli";
      case TREE_DATABASE_LOG:        return "log";
      default:                       return "???";
   }
}

static ZGPeerSettings GetTestTreeZGPeerSettings(const Message & args)
{
   // Just so we can see that this is working
   MessageRef peerAttributes = GetMessageFromPool();
   peerAttributes()->AddString("type", "tree_server");

   ZGPeerSettings s("tree_server", "test_tree_system", NUM_TREE_DATABASES, false);
   s.SetPeerAttributes(peerAttributes);

   String multicastMode;
   if (args.FindString("multicast", multicastMode).IsOK())
   {
      if (multicastMode.ContainsIgnoreCase("sim"))
      {
         LogTime(MUSCLE_LOG_INFO, "Forcing all network interfaces to use SimulatedMulticastDataIO!\n");
         s.SetMulticastBehavior(ZG_MULTICAST_BEHAVIOR_SIMULATED_ONLY);      
      }
      else if (multicastMode.ContainsIgnoreCase("standard"))
      {
         LogTime(MUSCLE_LOG_INFO, "Forcing all network interfaces to use real multicast DataIO!\n");
         s.SetMulticastBehavior(ZG_MULTICAST_BEHAVIOR_STANDARD_ONLY);      
      }
   }

   String maxLogSizeBytesStr;
   if (args.FindString("maxlogsizebytes", maxLogSizeBytesStr).IsOK())
   {
      uint32 maxBytes = atol(maxLogSizeBytesStr());
      if (maxBytes > 0)
      {
         LogTime(MUSCLE_LOG_INFO, "Setting maximum log size for database #0 to " UINT32_FORMAT_SPEC " bytes.\n", maxBytes);
         s.SetMaximumUpdateLogSizeForDatabase(0, maxBytes);
      }
      else LogTime(MUSCLE_LOG_WARNING, "maxlogsizebytes argument didn't contain a value greater than zero, ignoring it.\n");
   }

   return s;
}

// This class implements a database-peer to test out the MessageTreeDatabaseObject class
class TestTreeZGPeerSession : public MessageTreeDatabasePeerSession
{
public:
   TestTreeZGPeerSession(const Message & args) : MessageTreeDatabasePeerSession(GetTestTreeZGPeerSettings(args)), _acceptPort(0) {/* empty */}

   virtual const char * GetTypeName() const {return "TestTreeZGPeer";}

   virtual bool TextCommandReceived(const String & text)
   {
      printf("TestTreeZGPeerSession:  You typed:  [%s]\n", text());

      if (text.StartsWith("printsessions")) {PrintFactoriesInfo(); PrintSessionsInfo();}
      else return false;

      return true;  // indicate handled
   }

   virtual uint64 HandleDiscoveryPing(MessageRef & pingMsg, const IPAddressAndPort & pingSource)
   {
      const uint64 ret = MessageTreeDatabasePeerSession::HandleDiscoveryPing(pingMsg, pingSource);
      if (ret != MUSCLE_TIME_NEVER) (void) pingMsg()->CAddInt16("port", _acceptPort);  // clients will want to know this!
      return ret; 
   }

   void SetAcceptPort(uint16 port) {_acceptPort = port;}  // just so we can tell discovery-clients what port we are listening on

protected:
   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase)
   {
      IDatabaseObjectRef ret(newnothrow MessageTreeDatabaseObject(this, whichDatabase, GetDatabaseRootPath(whichDatabase)));
      if (ret() == NULL) MWARN_OUT_OF_MEMORY;
      return ret;
   }

private:
   uint16 _acceptPort;
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

   // Our test_tree_peer business logic is all implemented inside this object
   TestTreeZGPeerSession zgPeerSession(args);

   // This object will read from stdin for us, so we can accept typed text commands from the user
   ZGStdinSession zgStdinSession(zgPeerSession, true);

   // Accept incoming TCP connections from clients
   ServerSideMessageTreeSessionFactory sssFactory(zgPeerSession.GetClientTreeGateway());

   // This object will respond to multicast discovery queries sent across the LAN by clients, so that
   // they can find us without knowing our IP address and port in advance
   DiscoveryServerSession sdss(zgPeerSession);

   // This object implements the standard MUSCLE event loop and network services
   ReflectServer server;

   // Since we want to be able to run multiple servers at once, we'll keep trying to bind to a port until
   // we find one that's open.
   status_t ret;
   uint16 acceptPort = TREE_PEER_SERVER_PORT;
   while(server.PutAcceptFactory(acceptPort, ReflectSessionFactoryRef(&sssFactory, false)).IsError(ret))
   {
      if ((acceptPort-TREE_PEER_SERVER_PORT) >= 1000)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Too many ports attempted without success, giving up!\n");
         return 10;
      }

      LogTime(MUSCLE_LOG_WARNING, "Couldn't bind to port %u (%s); trying the next port...\n", acceptPort, ret());
      ret = B_NO_ERROR;  // clear the error-flag
      acceptPort++;
   }
   zgPeerSession.SetAcceptPort(acceptPort);
   LogTime(MUSCLE_LOG_INFO, "Listening for incoming client TCP connections (from tree_client) on port %u\n", acceptPort);

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   if (((IsDaemonProcess())||(server.AddNewSession(ZGStdinSessionRef(&zgStdinSession, false)).IsOK(ret)))&&
       (server.AddNewSession(ZGPeerSessionRef(&zgPeerSession, false)).IsOK(ret))&&
       (server.AddNewSession(DiscoveryServerSessionRef(&sdss, false)).IsOK(ret)))
   {
      // Virtually all of the program's execution time happens inside the ServerProcessLoop() method
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


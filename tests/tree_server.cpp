#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"

#include "zg/ZGDatabasePeerSession.h"
#include "zg/ZGStdinSession.h"
#include "zg/ZGMessageTreeDatabaseObject.h"
#include "zg/gateway/tree/TreeServerSideSession.h"

using namespace zg;

enum {
   TREE_DB_COMMAND_SET_DB_STATE = 1953654117, // 'toyd' -- clears the current DB state and then adds the specified key/value pairs
   TREE_DB_COMMAND_PUT_STRINGS,               //        -- adds the specified key/value pairs (overwriting the value of any existing keys that match the new keys)
   TREE_DB_COMMAND_REMOVE_STRINGS,            //        -- removes any key/value pairs whose keys match those found in the Message
   TREE_DB_COMMAND_USER_TEXT,                 //        -- just some chat text to print when received, for testing
};

// Some example-databases we might want to manage separately from each other
enum {
   TREE_DATABASE_USERDATA = 0,  // user's data goes here
   TREE_DATABASE_SYSTEMINFO,    // system's information about its current state goes here
   TREE_DATABASE_LOG,           // system log messages go here
   NUM_TREE_DATABASES
};

static ZGPeerSettings GetTestTreeZGPeerSettings(const Message & args)
{
   // Just so we can see that this is working
   MessageRef peerAttributes = GetMessageFromPool();
   peerAttributes()->AddString("testing", "attributes");
   peerAttributes()->AddInt32("some_value", (GetRunTime64()%10000));
   peerAttributes()->AddFloat("pi", 3.14159f);

   ZGPeerSettings s("test", NUM_TREE_DATABASES, false);
   s.SetPeerAttributes(peerAttributes);

   String multicastMode;
   if (args.FindString("multicast", multicastMode) == B_NO_ERROR)
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
   if (args.FindString("maxlogsizebytes", maxLogSizeBytesStr) == B_NO_ERROR)
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

// This class implements a database-peer to test out the ZGMessageTreeDatabaseObject class
class TestTreeZGPeerSession : public ZGDatabasePeerSession
{
public:
   TestTreeZGPeerSession(const Message & args) : ZGDatabasePeerSession(GetTestTreeZGPeerSettings(args)) {/* empty */}

   virtual const char * GetTypeName() const {return "TestTreeZGPeer";}

   virtual bool TextCommandReceived(const String & text)
   {
      printf("You typed:  [%s]\n", text());
      return true;  // indicate handled?
   }

protected:
   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase)
   {
      IDatabaseObjectRef ret(newnothrow ZGMessageTreeDatabaseObject(String("dbs/db_%1").Arg(whichDatabase)));
      if (ret() == NULL) WARN_OUT_OF_MEMORY;
      return ret;
   }

#ifdef TEMP_REMOVE
   virtual void MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg)
   {
      printf("Received incoming Message from peer [%s]:\n", fromPeerID.ToString()());
      msg()->PrintToStream();
   }
#endif

private:
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
   TreeServerSideSessionFactory sssFactory(&zgPeerSession);

   // This object implements the standard MUSCLE event loop and network services
   ReflectServer server;

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   status_t ret;
   if (((IsDaemonProcess())||(server.AddNewSession(ZGStdinSessionRef(&zgStdinSession, false)).IsOK(ret)))&&
       (server.PutAcceptFactory(TREE_PEER_SERVER_PORT, ReflectSessionFactoryRef(&sssFactory, false)).IsOK(ret))&&
       (server.AddNewSession(ZGPeerSessionRef(&zgPeerSession, false)).IsOK(ret)))
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


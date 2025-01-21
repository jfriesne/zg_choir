#include "reflector/ReflectServer.h"
#include "zg/ZGConstants.h"   // for GetRandomNumber()
#include "zg/ZGPeerSettings.h"
#include "zg/discovery/server/DiscoveryServerSession.h"
#include "zg/messagetree/server/ClientDataMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "zg/messagetree/server/UndoStackMessageTreeDatabaseObject.h"
#include "common/FridgeConstants.h"

namespace fridge {

static const char * _magnetWordsList[] = {
#include "common_words_list.txt"
};

static String GetNextRandomMagnetWord()
{
   static uint32 _nextMagnetWordIndex = 0;

   const String ret = _magnetWordsList[_nextMagnetWordIndex];
   _nextMagnetWordIndex = (_nextMagnetWordIndex+1)%ARRAYITEMS(_magnetWordsList);
   return ret;
}

enum {
   FRIDGE_DB_PROJECT = 0,  // the project info (including undo/redo state, and the current state of the refrigerator-magnets is stored here under "project")
   FRIDGE_DB_CHAT,         // chat text (under subtree "chat")
   FRIDGE_DB_CLIENTS,      // the set of currently-connected clients is stored here (under subtree "clients/")
   NUM_FRIDGE_DBS          // guard value
};

static ZGPeerSettings GetFridgePeerSettings(const String & systemName)
{
   ZGPeerSettings settings(FRIDGE_PROGRAM_SIGNATURE, systemName, NUM_FRIDGE_DBS, false);
   settings.SetMaximumUpdateLogSizeForDatabase(FRIDGE_DB_PROJECT, 256*1024);  // setting it small just to make it easier to test undo-handling
   settings.SetApplicationPeerCompatibilityVersion(FRIDGE_APP_COMPATIBILITY_VERSION);
   return settings;
}

// This class implements a database-peer to test out the MessageTreeDatabaseObject class
class FridgePeerSession : public MessageTreeDatabasePeerSession
{
public:
   FridgePeerSession(const String & systemName) : MessageTreeDatabasePeerSession(GetFridgePeerSettings(systemName)), _acceptPort(0) {/* empty */}

   virtual const char * GetTypeName() const {return "FridgePeer";}

   virtual bool TextCommandReceived(const String & text)
   {
      printf("FridgePeerSession:  You typed:  [%s]\n", text());

      if (text.StartsWith("printsessions"))
      {
         PrintFactoriesInfo(stdout);
         PrintSessionsInfo(stdout);
         return true;  // indicate handled
      }
      else return false;
   }

   virtual uint64 HandleDiscoveryPing(MessageRef & pingMsg, const IPAddressAndPort & pingSource)
   {
      uint64 ret = MessageTreeDatabasePeerSession::HandleDiscoveryPing(pingMsg, pingSource);
      if (ret != MUSCLE_TIME_NEVER)
      {
         if (pingMsg()->CAddInt16("port", _acceptPort).IsError()) return MUSCLE_TIME_NEVER;  // clients will want to know what port they should connect to!

         uint32 numConnectedFridgeClients = 0;
         for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
            if (dynamic_cast<ServerSideMessageTreeSession *>(iter.GetValue()()) != NULL) numConnectedFridgeClients++;

         // Have FridgeServers with lots of connected clients respond a bit slower, so that new FridgeClients will tend
         // to connect to the less-loaded FridgeServers to encourage some rough load-balancing.
         ret = muscleClamp((int64) numConnectedFridgeClients*MillisToMicros(10), (int64)0, (int64)MillisToMicros(200));
      }
      return ret;
   }

   void SetAcceptPort(uint16 port) {_acceptPort = port;}  // just so we can tell discovery-clients what port we are listening on

protected:
   // Same as an UndoStackMessageTreeDatabaseObject, but creates the "magnets" node as part of the SetToDefaultState() method
   class MagnetsMessageTreeDatabaseObject : public UndoStackMessageTreeDatabaseObject
   {
   public:
      MagnetsMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) : UndoStackMessageTreeDatabaseObject(session, dbIndex, rootNodePath)
      {
         // empty
      }

      virtual void SetToDefaultState()
      {
         UndoStackMessageTreeDatabaseObject::SetToDefaultState();  // clear any existing nodes

         status_t ret;
         if (SetDataNode("magnets", GetMessageFromPool()).IsError(ret)) LogTime(MUSCLE_LOG_CRITICALERROR, "MagnetsMessageTreeDatabaseObject::SetToDefaultState():  Couldn't set magnets node! [%s]\n", ret());
      }

      virtual void MessageReceivedFromTreeGatewaySubscriber(const ZGPeerID & fromPeerID, const MessageRef & payload, const String & tag)
      {
         switch(payload()->what)
         {
            case FRIDGE_COMMAND_GETRANDOMWORD:
            {
               payload()->what = FRIDGE_REPLY_RANDOMWORD;  // we'll just send the command-Message back as the reply, so that the caller gets back any fields he sent us

               status_t ret;
               if ((payload()->AddString(FRIDGE_NAME_WORD, GetNextRandomMagnetWord()).IsError(ret)) ||
                   (SendMessageToTreeGatewaySubscriber(fromPeerID, tag, payload).IsError(ret)))
               {
                  LogTime(MUSCLE_LOG_ERROR, "MagnetsMessageTreeDatabaseObject:  Error sending random-word reply to subscriber at [%s] [%s] [%s]\n", fromPeerID.ToString()(), tag(), ret());
               }
            }
            break;

            default:
               UndoStackMessageTreeDatabaseObject::MessageReceivedFromTreeGatewaySubscriber(fromPeerID, payload, tag);
            break;
         }
      }
   };

   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase)
   {
      switch(whichDatabase)
      {
         case FRIDGE_DB_PROJECT: return IDatabaseObjectRef(new MagnetsMessageTreeDatabaseObject(   this, whichDatabase, "project"));
         case FRIDGE_DB_CHAT:    return IDatabaseObjectRef(new MessageTreeDatabaseObject(          this, whichDatabase, "chat"));
         case FRIDGE_DB_CLIENTS: return IDatabaseObjectRef(new ClientDataMessageTreeDatabaseObject(this, whichDatabase, "clients"));

         default:
            LogTime(MUSCLE_LOG_CRITICALERROR, "FridgePeerSession::CreateDatabaseObject(" UINT32_FORMAT_SPEC "):  Unknown database ID!\n", whichDatabase);
            return IDatabaseObjectRef();
         break;
      }
   }

private:
   uint16 _acceptPort;
};

int RunFridgeServerProcess(const char * systemName)
{
   int exitCode = 10;

   // shuffle the words list into a random order
   {
      unsigned int seed = time(NULL);
      for (uint32 i=0; i<ARRAYITEMS(_magnetWordsList); i++) muscleSwap(_magnetWordsList[i], _magnetWordsList[GetRandomNumber(&seed)%ARRAYITEMS(_magnetWordsList)]);
   }

   // This object is required by the MUSCLE library;
   // it does various system-specific startup and shutdown tasks
   CompleteSetupSystem css;

   // Our FridgeServer business logic is all implemented inside this object
   FridgePeerSession fridgePeerSession(systemName);

   // This object will read from stdin for us, so we can accept typed text commands from the user
   ZGStdinSession stdinSession(fridgePeerSession, true);

   // This factory will accept incoming TCP connections from FridgeClients
   ServerSideMessageTreeSessionFactory sssFactory(fridgePeerSession.GetClientTreeGateway(), true);

   // This object will respond to multicast discovery queries sent across the LAN by clients, so that
   // they can find us without knowing our IP address and port in advance
   DiscoveryServerSession sdss(fridgePeerSession);

   // This object implements the standard MUSCLE event loop and network services
   ReflectServer server;

   // Allocate a TCP port to accept incoming client connections on
   status_t ret;
   uint16 acceptPort;
   if (server.PutAcceptFactory(0, DummyReflectSessionFactoryRef(sssFactory), invalidIP, &acceptPort).IsError(ret))
   {
      LogTime(MUSCLE_LOG_WARNING, "Couldn't bind to a TCP port to accept incoming connections, exiting!\n");
      return exitCode;
   }
   fridgePeerSession.SetAcceptPort(acceptPort);

   LogTime(MUSCLE_LOG_INFO, "Listening for incoming client TCP connections (from FridgeClient) on port %u\n", acceptPort);

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   if ((server.AddNewSession(DummyZGStdinSessionRef(stdinSession)).IsOK(ret))
     &&(server.AddNewSession(DummyZGPeerSessionRef(fridgePeerSession)).IsOK(ret))
     &&(server.AddNewSession(DummyDiscoveryServerSessionRef(sdss)).IsOK(ret)))
   {
      // Virtually all of the program's execution time happens inside the ServerProcessLoop() method
      ret = server.ServerProcessLoop();  // doesn't return until it's time to exit
      if (ret.IsOK())
      {
         LogTime(MUSCLE_LOG_INFO, "Event loop exited normally.\n");
         exitCode = 0;
      }
      else LogTime(MUSCLE_LOG_ERROR, "Event loop exited with error [%s]\n", ret());
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set up sessions [%s]!\n", ret());

   // Required in order to ensure an orderly shutdown
   server.Cleanup();

   return exitCode;
}

};

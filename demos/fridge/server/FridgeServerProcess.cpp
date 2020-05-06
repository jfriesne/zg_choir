#include "reflector/ReflectServer.h"
#include "zg/ZGPeerSettings.h"
#include "zg/discovery/server/DiscoveryServerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"
#include "common/FridgeNameSpace.h"

namespace fridge {

static ZGPeerSettings GetFridgePeerSettings(const String & systemName)
{
   return ZGPeerSettings("Fridge", systemName, 1, false);
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
      if (whichDatabase != 0)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "FridgePeerSession only supports one database, for now\n");
         return IDatabaseObjectRef();
      }

      IDatabaseObjectRef ret(newnothrow MessageTreeDatabaseObject(this, whichDatabase, GetEmptyString()));
      if (ret() == NULL) WARN_OUT_OF_MEMORY;
      return ret;
   }

private:
   uint16 _acceptPort;
};

int RunFridgeServerProcess(const char * systemName)
{
   int exitCode = 10;

   // This object is required by the MUSCLE library;
   // it does various system-specific startup and shutdown tasks
   CompleteSetupSystem css;

   // Our FridgeServer business logic is all implemented inside this object
   FridgePeerSession fridgePeerSession(systemName);

   // This object will read from stdin for us, so we can accept typed text commands from the user
   ZGStdinSession stdinSession(fridgePeerSession, true);

   // This factory will accept incoming TCP connections from FridgeClients
   ServerSideMessageTreeSessionFactory sssFactory(fridgePeerSession.GetClientTreeGateway());

   // This object will respond to multicast discovery queries sent across the LAN by clients, so that
   // they can find us without knowing our IP address and port in advance
   DiscoveryServerSession sdss(fridgePeerSession);

   // This object implements the standard MUSCLE event loop and network services
   ReflectServer server;

   // Since we want to be able to run multiple servers at once, we'll keep trying to bind to a port until
   // we find one that's open.
   status_t ret;
   uint16 acceptPort;
   while(server.PutAcceptFactory(0, ReflectSessionFactoryRef(&sssFactory, false), invalidIP, &acceptPort).IsError(ret))
   {
      LogTime(MUSCLE_LOG_WARNING, "Couldn't bind to a TCP port to accept incoming connections, exiting!\n");
      return exitCode;
   }
   fridgePeerSession.SetAcceptPort(acceptPort);
   LogTime(MUSCLE_LOG_INFO, "Listening for incoming client TCP connections (from FridgeClient) on port %u\n", acceptPort);

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   if ((server.AddNewSession(ZGStdinSessionRef(        &stdinSession,      false)).IsOK(ret))
     &&(server.AddNewSession(ZGPeerSessionRef(         &fridgePeerSession, false)).IsOK(ret))
     &&(server.AddNewSession(DiscoveryServerSessionRef(&sdss,              false)).IsOK(ret)))
   {
      // Virtually all of the program's execution time happens inside the ServerProcessLoop() method
      ret = server.ServerProcessLoop();  // doesn't return until it's time to exit
      if (ret == B_NO_ERROR)
      {
         LogTime(MUSCLE_LOG_INFO, "Event loop exited normally.\n");
         exitCode = 0;
      }
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set up sessions [%s]!\n", ret());

   // Required in order to ensure an orderly shutdown
   server.Cleanup();

   return exitCode;
}

};

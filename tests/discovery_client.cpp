#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/SocketCallbackMechanism.h"

#include "zg/ZGStdinSession.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

using namespace zg;

class TestDiscoveryNotificationTarget : public IDiscoveryNotificationTarget
{
public:
   TestDiscoveryNotificationTarget(SystemDiscoveryClient * client) : IDiscoveryNotificationTarget(client)
   {
      // empty
   }

   virtual void DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo)
   {
      printf("\n\n");
      if (optSystemInfo())
      {
         LogTime(MUSCLE_LOG_INFO, "Discovery update for system [%s]:\n", systemName());
         optSystemInfo()->Print(OutputPrinter(stdout).WithIndent());
      }
      else LogTime(MUSCLE_LOG_INFO, "System [%s] has gone offline!\n", systemName());
   }

   virtual void ComputerIsAboutToSleep()
   {
      printf("\n\n");
      LogTime(MUSCLE_LOG_INFO, "This computer is about to go to sleep!\n");
   }

   virtual void ComputerJustWokeUp()
   {
      printf("\n\n");
      LogTime(MUSCLE_LOG_INFO, "This computer just woke up.\n");
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;     // set up MUSCLE environment
   SocketCallbackMechanism scm; // orchestrates safe calling of callback-methods in the main/user/GUI thread

   AndQueryFilterRef discoFilter(new AndQueryFilter);  // to limit results to only the servers we care about
   (void) discoFilter()->GetChildren().AddTail(ConstQueryFilterRef(new StringQueryFilter("type", StringQueryFilter::OP_EQUAL_TO, "tree_server")));

   status_t ret;
   SystemDiscoveryClient discoveryClient(&scm, "*", discoFilter);
   if (discoveryClient.Start().IsOK(ret))
   {
      TestDiscoveryNotificationTarget testTarget(&discoveryClient);

      LogTime(MUSCLE_LOG_INFO, "Listening for on-line systems...\n");

      SocketMultiplexer sm;
      while(true)
      {
         (void) sm.RegisterSocketForReadReady(scm.GetDispatchThreadNotifierSocket().GetFileDescriptor());
         if (sm.WaitForEvents().IsOK(ret)) scm.DispatchCallbacks();
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed, exiting! [%s]\n", ret());
            break;
         }
      }
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't start SystemDiscoveryClient, exiting! [%s]\n", ret());

   return 0;
}

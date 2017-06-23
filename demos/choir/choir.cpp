#include <QApplication>

#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for HandleStandardDaemonArgs()

#include "ChoirWindow.h"

using namespace choir;

#ifdef __APPLE__
extern void disable_app_nap(void);
#endif

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;  // necessary for MUSCLE initialization

#ifdef __APPLE__
   // otherwise MacOS/X will start putting our heartbeat threads
   // to sleep when the choir window is occluded, causing other
   // choir members to think we've gone offline when we haven't.
   disable_app_nap();
#endif

   {
      Message args; (void) ParseArgs(argc, argv, args);
      HandleStandardDaemonArgs(args);
   }

   QApplication app(argc, argv);

   ChoirWindow * win = new ChoirWindow;  // allocated on heap so that WA_DeleteOnClose can work
   win->show();

   return app.exec();
}

#include <QApplication>

#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for HandleStandardDaemonArgs()

#include "ChoirWindow.h"

using namespace choir;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;  // necessary for MUSCLE initialization

   {
      Message args; (void) ParseArgs(argc, argv, args);
      HandleStandardDaemonArgs(args);
   }

   QApplication app(argc, argv);

   ChoirWindow * win = new ChoirWindow;  // allocated on heap so that WA_DeleteOnClose can work
   win->show();

   return app.exec();
}

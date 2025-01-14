#include <QApplication>
#include "FridgeServerWindow.h"

namespace fridge {
extern int RunFridgeServerProcess(const char * systemName);
};

int main(int argc, char ** argv)
{
   using namespace fridge;

   if ((argc >= 2)&&(strncmp(argv[1], "systemname=", 11) == 0))
   {
      // We're running as a server sub-process (presumably we were launched by the GUI)
      return RunFridgeServerProcess(argv[1]+11);
   }
   else
   {
      CompleteSetupSystem css;
      QApplication app(argc, argv);

      FridgeServerWindow * fsw = new FridgeServerWindow(argv[0]);  // must be on the heap since we call setAttribute(Qt::WA_DeleteOnClose) in the FridgeServerWindow constructor
      fsw->SetServerRunning(true);
      fsw->show();
      return app.exec();
   }
}

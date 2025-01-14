#include <QApplication>
#include "platform/qt/QSocketCallbackMechanism.h"
#include "FridgeClientWindow.h"

int main(int argc, char ** argv)
{
   using namespace fridge;

   CompleteSetupSystem css;
   QApplication app(argc, argv);

   // Network I/O threads will rely on this object to call callback
   // methods safely from within the context of the main/GUI/Qt thread.
   QSocketCallbackMechanism qcbm;

   // must be on the heap since we call
   // setAttribute(Qt::WA_DeleteOnClose) in the FridgeClientWindow constructor
   FridgeClientWindow * fsw = new FridgeClientWindow(&qcbm);
   fsw->show();

   return app.exec();
}

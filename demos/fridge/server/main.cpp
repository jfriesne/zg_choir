#include <QApplication>
#include "system/Thread.h"
#include "FridgeServerWindow.h"

namespace fridge {
extern int RunFridgeServerProcess(const char * systemName);
};

 
class FridgeServerThread : public muscle::Thread
{
public:
   FridgeServerThread() {}

   void SetSystemName(const muscle::String & sn) {_systemName = sn;}

private:
   muscle::String _systemName;

   virtual void InternalThreadEntry()
   {
      printf("Thread %p:  running server process for system [%s]\n", this, _systemName());
      (void) fridge::RunFridgeServerProcess(_systemName());
      printf("Thread %p:  RunFridgeServerProcess() returned.\n", this);
   }
};

int main(int argc, char ** argv)
{
   using namespace fridge;

   if ((argc >= 2)&&(strncmp(argv[1], "systemname=", 11) == 0)) 
   {
      // We're running as a server sub-process (presumably we were launched by the GUI)
      const uint32 NUM_THREADS=5;
      printf("Launching " UINT32_FORMAT_SPEC " threads\n", NUM_THREADS);

      FridgeServerThread threads[NUM_THREADS];
      for (uint32 i=0; i<ARRAYITEMS(threads); i++)
      {
         threads[i].SetSystemName(argv[1]+11);
         if (threads[i].StartInternalThread().IsError()) LogTime(MUSCLE_LOG_ERROR, "Error starting FridgeServerThread!\n");
      }
      printf("Threads are running!\n");
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) threads[i].WaitForInternalThreadToExit();
      return 0;
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

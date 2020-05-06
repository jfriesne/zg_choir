#ifndef FridgeServerWindow_h
#define FridgeServerWindow_h

#include <QMainWindow>

#include "dataio/ChildProcessDataIO.h"
#include "common/FridgeNameSpace.h"
#include "qtsupport/QDataIODevice.h"
#include "util/String.h"

class QPlainTextEdit;
class QPushButton;
class QLineEdit;

namespace fridge {

/** This is a thin GUI-layer to wrap around a Fridge server process */
class FridgeServerWindow : public QMainWindow
{
Q_OBJECT

public:
   /** Default constructor */
   FridgeServerWindow(const String & argv0);

   /** Destructor */
   virtual ~FridgeServerWindow();

public slots:
   void SetServerRunning(bool running);

private slots:
   void StartServer() {SetServerRunning(true);}
   void StopServer()  {SetServerRunning(false);}
   void ClearLog();
   void CloneServer();
   void UpdateStatus();
   void ReadChildProcessOutput();

private:
   const String _argv0;

   QPushButton * _startButton;
   QPushButton * _stopButton;
   QLineEdit   * _systemName;

   QPlainTextEdit * _serverOutput;

   QPushButton * _cloneWindowButton;
   QPushButton * _clearButton;

   ChildProcessDataIORef _childProcess;
   QDataIODevice * _childProcessIODevice;
};

}; // end namespace fridge

#endif

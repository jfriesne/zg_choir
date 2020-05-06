#ifndef FridgeServerWindow_h
#define FridgeServerWindow_h

#include <QMainWindow>
#include <QThread>

#include "dataio/ChildProcessDataIO.h"
#include "common/FridgeNameSpace.h"

class QPlainTextEdit;
class QPushButton;
class QLineEdit;

namespace fridge {

class MusicSheetWidget;
class RosterWidget;

/** This is a thin GUI-layer to wrap around a Fridge server process */
class FridgeServerWindow : public QMainWindow
{
Q_OBJECT

public:
   /** Default constructor */
   FridgeServerWindow();

   /** Destructor */
   virtual ~FridgeServerWindow();

private slots:
   void StartServer() {SetServerRunning(true);}
   void StopServer()  {SetServerRunning(false);}
   void ClearLog();
   void CloneServer();
   void UpdateStatus();

private:
   void SetServerRunning(bool running);

   QPushButton * _startButton;
   QPushButton * _stopButton;
   QLineEdit   * _serverName;

   QPlainTextEdit * _serverOutput;

   QPushButton * _cloneWindowButton;
   QPushButton * _clearButton;

   ChildProcessDataIORef _childProcess;
};

}; // end namespace fridge

#endif

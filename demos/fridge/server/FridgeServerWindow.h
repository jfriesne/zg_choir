#ifndef FridgeServerWindow_h
#define FridgeServerWindow_h

#include <QByteArray>
#include <QMainWindow>
#include <QTimer>

#include "dataio/ChildProcessDataIO.h"
#include "common/FridgeNameSpace.h"
#include "qtsupport/QDataIODevice.h"
#include "util/String.h"
#include "zg/ZGPeerID.h"

class QPlainTextEdit;
class QPushButton;
class QLineEdit;

namespace fridge {

/** This is a thin GUI-layer to wrap around a Fridge server process */
class FridgeServerWindow : public QMainWindow
{
Q_OBJECT

public:
   /** Default constructor
     * @param argv0 the first entry of the argv[] array that was passed in to main().  (Used when launching a child process)
     */
   FridgeServerWindow(const String & argv0);

   /** Destructor */
   virtual ~FridgeServerWindow();

public slots:
   /** Launches or killed the child process running the actual server code.
     * @param running true iff the child process should be running, false if it should not be.
     * @note this method has no effect if the child process is already in the desired state.
     */
   void SetServerRunning(bool running);

private slots:
   void StartServer() {SetServerRunning(true);}
   void StopServer()  {SetServerRunning(false);}
   void ClearLog();
   void CloneServer();
   void UpdateStatus();
   void ReadChildProcessOutput();
   void ScheduleUpdateStatus();

private:
   void ParseTextLinesFromIncomingTextBuffer();
   void ParseIncomingTextLine(const String & t);

   const String _argv0;

   QPushButton * _startButton;
   QPushButton * _stopButton;
   QLineEdit   * _systemName;

   QPlainTextEdit * _serverOutput;

   QPushButton * _cloneWindowButton;
   QPushButton * _clearButton;

   ChildProcessDataIORef _childProcess;
   QDataIODevice * _childProcessIODevice;

   QByteArray _incomingTextBuffer;

   ZGPeerID _peerID;
   uint16 _port;

   bool _updateStatusPending;
};

}; // end namespace fridge

#endif

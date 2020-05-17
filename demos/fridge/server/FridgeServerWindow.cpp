#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

#include "FridgeServerWindow.h"

namespace fridge {

FridgeServerWindow :: FridgeServerWindow(const String & argv0) 
   : _argv0(argv0)
   , _childProcessIODevice(NULL)
   , _port(0)
   , _clientCount(0)
   , _updateStatusPending(false)
{
   setWindowTitle(tr("Fridge Server"));
   setAttribute(Qt::WA_DeleteOnClose);
   resize(800, 600);

   QWidget * central = new QWidget;
   setCentralWidget(central);

   QBoxLayout * vbl = new QBoxLayout(QBoxLayout::TopToBottom, central);
   vbl->setSpacing(3);
   vbl->setMargin(2);

   QWidget * headerLine = new QWidget;
   {
      QBoxLayout * headerLayout = new QBoxLayout(QBoxLayout::LeftToRight, headerLine);

      _startButton = new QPushButton(tr("Start Server"));
      connect(_startButton, SIGNAL(clicked()), this, SLOT(StartServer()));
      headerLayout->addWidget(_startButton);

      _stopButton = new QPushButton(tr("Stop Server"));
      connect(_stopButton, SIGNAL(clicked()), this, SLOT(StopServer()));
      headerLayout->addWidget(_stopButton);

      QLabel * systemNameLabel = new QLabel(tr("System Name: "));
      headerLayout->addWidget(systemNameLabel);

      _systemName = new QLineEdit;
      _systemName->setFocusPolicy(Qt::ClickFocus);
      _systemName->setText(tr("Default Fridge"));
      headerLayout->addWidget(_systemName, 1);
   }
   vbl->addWidget(headerLine);

   _serverOutput = new QPlainTextEdit;
   _serverOutput->setReadOnly(true);
   vbl->addWidget(_serverOutput, 1);

   QWidget * footerLine = new QWidget;
   {
      QBoxLayout * footerLayout = new QBoxLayout(QBoxLayout::LeftToRight, footerLine);

      footerLayout->addStretch();

      _cloneWindowButton = new QPushButton(tr("Clone Server"));
      connect(_cloneWindowButton, SIGNAL(clicked()), this, SLOT(CloneServer()));
      footerLayout->addWidget(_cloneWindowButton); 

      footerLayout->addStretch();

      _peerIDLabel = new QLabel;
      _peerIDLabel->setAlignment(Qt::AlignCenter); 
      footerLayout->addWidget(_peerIDLabel); 
 
      footerLayout->addStretch();

      _clearButton = new QPushButton(tr("Clear Output"));
      connect(_clearButton, SIGNAL(clicked()), this, SLOT(ClearLog()));
      footerLayout->addWidget(_clearButton); 

      footerLayout->addStretch();
   }
   vbl->addWidget(footerLine);

   ScheduleUpdateStatus();
}

FridgeServerWindow :: ~FridgeServerWindow()
{
   SetServerRunning(false);
}

void FridgeServerWindow :: ClearLog()
{
   _serverOutput->clear();
}

void FridgeServerWindow :: CloneServer()
{
   FridgeServerWindow * clone = new FridgeServerWindow(_argv0);
   clone->_systemName->setText(_systemName->text());
   if (_childProcess() != NULL) clone->SetServerRunning(true);
   clone->show();
}

void FridgeServerWindow :: ScheduleUpdateStatus()
{
   if (_updateStatusPending == false) 
   {
      _updateStatusPending = true; 
      QTimer::singleShot(0, this, SLOT(UpdateStatus()));
   }
}

static int Mix(int v1, const int v2, float p) {return (int) ((((float)v2)*p)+(((float)v1)*(1.0f-p)));}
static QColor MixColors(const QColor & c1, const QColor & c2, float p) {return QColor(Mix(c1.red(), c2.red(), p), Mix(c1.green(), c2.green(), p), Mix(c1.blue(), c2.blue(), p));}

void FridgeServerWindow :: UpdateStatus()
{
   _updateStatusPending = false;

   _startButton->setEnabled(_childProcess() == NULL);
   _stopButton->setEnabled( _childProcess() != NULL);
   _systemName->setReadOnly(_childProcess() != NULL);

   const bool isRunning    = (_childProcess() != NULL);
   const bool isSeniorPeer = ((isRunning)&&(_peerID.IsValid())&&(_seniorPeerID == _peerID));
   QString windowTitle = isRunning ? (isSeniorPeer ? tr("SENIOR Fridge Server") : tr("Junior Fridge Server")) : tr("Disabled Fridge Server");
   if (isRunning)
   {
      if (_port != 0)
      {
         if (_clientCount > 0) windowTitle += tr(" (%1 %2 on Port %3)").arg(_clientCount).arg((_clientCount==1)?tr("client"):tr("clients")).arg(_port);
                          else windowTitle += tr(" (Port %1)").arg(_port);
      }
   }
   setWindowTitle(windowTitle);

   _peerIDLabel->setText(isRunning ? (_peerID.IsValid() ? tr("Peer ID:  %1").arg(_peerID.ToString()()) : QString()) : tr("Server stopped"));

   QPalette p = _serverOutput->palette();
   p.setColor(QPalette::Base, _childProcess()?MixColors(Qt::green,Qt::white,isSeniorPeer?0.90f:1.00f):QColor(Qt::lightGray));
   _serverOutput->setPalette(p);
}

void FridgeServerWindow :: ReadChildProcessOutput()
{
   if (_childProcessIODevice)
   {
      _incomingTextBuffer.append(_childProcessIODevice->readAll());
      ParseTextLinesFromIncomingTextBuffer();
      if ((_childProcessIODevice->isOpen() == false)||(_childProcessIODevice->atEnd())) SetServerRunning(false);
   }
}

void FridgeServerWindow :: ParseTextLinesFromIncomingTextBuffer()
{
   while(true)
   {
      const int nextNewlineIdx = _incomingTextBuffer.indexOf('\n');
      if (nextNewlineIdx >= 0)
      {
         ParseIncomingTextLine(String(_incomingTextBuffer.data(), nextNewlineIdx));
         _incomingTextBuffer = _incomingTextBuffer.mid(nextNewlineIdx+1);
      }
      else break;
   }
}

void FridgeServerWindow :: ParseIncomingTextLine(const String & t)
{
   if (t.Contains("Listening for incoming client TCP connections")) 
   {
      const char * onPort = strstr(t(), "on port ");
      if (onPort) {_port = atoi(onPort+8); ScheduleUpdateStatus();}
   }
   else if (t.Contains("Starting up as peer"))
   {
      const char * asPeer = strstr(t(), "as peer [");
      if (asPeer) {_peerID.FromString(asPeer+9); ScheduleUpdateStatus();}
   }
   else if ((t.Contains("Senior peer has changed from ["))||(t.Contains("Senior peer has been set to [")))
   {
      const char * lastLeftBracket = strrchr(t(), '[');
      if (lastLeftBracket) {_seniorPeerID.FromString(lastLeftBracket+1); ScheduleUpdateStatus();}
   }
   else if (t.Contains("Client at"))
   {
           if (t.Contains("has connected to this server"))      {_clientCount++; ScheduleUpdateStatus();}
      else if (t.Contains("has disconnected from this server")) {_clientCount--; ScheduleUpdateStatus();}
   }

   _serverOutput->appendPlainText(t());
}

void FridgeServerWindow :: SetServerRunning(bool running)
{
   if (running != (_childProcess() != NULL))
   {
      _peerID       = ZGPeerID();
      _seniorPeerID = ZGPeerID();
      _port         = 0;
      _clientCount  = 0;

      if (running)
      {
         ChildProcessDataIORef cpdioRef(new ChildProcessDataIO(false));
         cpdioRef()->SetChildProcessShutdownBehavior(true, -1, SecondsToMicros(5));  // wait 5 seconds for the child process to exit voluntarily, then kill him

         Queue<String> argv;
         argv.AddTail(_argv0);
         argv.AddTail(String("systemname=%1").Arg(_systemName->text().trimmed().toUtf8().constData()));

         status_t ret;
         if (cpdioRef()->LaunchChildProcess(argv).IsOK(ret))
         {
            _childProcessIODevice = new QDataIODevice(cpdioRef, this);
            if (_childProcessIODevice->open(QIODevice::ReadWrite)) 
            {
               ClearLog();
               QObject::connect(_childProcessIODevice, SIGNAL(readyRead()), this, SLOT(ReadChildProcessOutput()));
               _childProcess = cpdioRef;
            }
            else 
            {
               LogTime(MUSCLE_LOG_CRITICALERROR, "Error, couldn't open QDataIODevice to child process!\n");
               delete _childProcessIODevice;
               _childProcessIODevice = NULL;
            }
         }
         else LogTime(MUSCLE_LOG_CRITICALERROR, "Error launching child process! [%s]\n", ret());
      }
      else 
      {
         delete _childProcessIODevice;
         _childProcess.Reset();
      }

      ScheduleUpdateStatus();
   }
}

}; // end namespace fridge

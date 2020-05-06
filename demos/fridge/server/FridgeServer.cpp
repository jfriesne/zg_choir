#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

#include "zg/ZGPeerSettings.h"
#include "FridgeServer.h"

namespace fridge {

static ZGPeerSettings GetFridgePeerSettings()
{
   ZGPeerSettings settings("Fridge", "Fridge", 1, false);
   return settings;
}

FridgeServerWindow :: FridgeServerWindow() 
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

      QLabel * serverNameLabel = new QLabel(tr("System Name: "));
      headerLayout->addWidget(serverNameLabel);

      _serverName = new QLineEdit;
      _serverName->setText(tr("Default Fridge"));
      headerLayout->addWidget(_serverName, 1);
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

      _clearButton = new QPushButton(tr("Clear Output"));
      connect(_clearButton, SIGNAL(clicked()), this, SLOT(ClearLog()));
      footerLayout->addWidget(_clearButton); 

      footerLayout->addStretch();
   }
   vbl->addWidget(footerLine);

   UpdateStatus();
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
   FridgeServerWindow * clone = new FridgeServerWindow;
   clone->_serverName->setText(_serverName->text());
   if (_childProcess() != NULL) clone->SetServerRunning(true);
   clone->show();
}

void FridgeServerWindow :: UpdateStatus()
{
   _startButton->setEnabled(_childProcess() == NULL);
   _stopButton->setEnabled( _childProcess() != NULL);
   _serverName->setReadOnly(_childProcess() != NULL);
}

void FridgeServerWindow :: SetServerRunning(bool running)
{
   if (running != (_childProcess() != NULL))
   {
      if (running)
      {
printf("Launch child process!\n");
      }
      else _childProcess.Reset();

      UpdateStatus();
   }
}

}; // end namespace fridge

int main(int argc, char ** argv)
{
   using namespace fridge;

   CompleteSetupSystem css;
   QApplication app(argc, argv);

   FridgeServerWindow * fsw = new FridgeServerWindow;  // must be on the heap since we call setAttribute(Qt::WA_DeleteOnClose) in the FridgeServerWindow constructor
   fsw->show();
   return app.exec();
}

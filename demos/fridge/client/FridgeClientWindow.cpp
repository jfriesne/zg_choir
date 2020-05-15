#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>

#include "FridgeClientCanvas.h"
#include "FridgeClientWindow.h"
#include "zg/ZGPeerID.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for ZG_DISCOVERY_NAME_PEERINFO

namespace fridge {

class FridgeClientCanvas;

FridgeClientWindow :: FridgeClientWindow(ICallbackMechanism * callbackMechanism) 
   : IDiscoveryNotificationTarget(NULL)  // can't pass in &_discoClient here, it isn't constructed yet!
   , _discoClient(callbackMechanism, FRIDGE_PROGRAM_SIGNATURE)
   , _canvasPage(NULL)
   , _connection(NULL)
   , _canvas(NULL)
{
   SetDiscoveryClient(&_discoClient);

   setAttribute(Qt::WA_DeleteOnClose);
   resize(800, 600);

   _widgetStack = new QStackedWidget;
   {
      setCentralWidget(_widgetStack);

      QLabel * noResults = new QLabel(tr("Searching for Fridge-systems..."));
      {
         noResults->setAlignment(Qt::AlignCenter);
      }
      _widgetStack->addWidget(noResults);

      QWidget * resultsListPage = new QWidget;
      {
         QBoxLayout * rlpLayout = new QBoxLayout(QBoxLayout::TopToBottom, resultsListPage);
         rlpLayout->setSpacing(5);
         rlpLayout->addStretch();

         QLabel * lab = new QLabel(tr("Choose a Fridge-system to connect to:"));
         lab->setAlignment(Qt::AlignCenter);
         rlpLayout->addWidget(lab);

         _systemsList = new QListWidget;
         connect(_systemsList, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(SystemItemClicked(QListWidgetItem *)));
         rlpLayout->addWidget(_systemsList);
         rlpLayout->addStretch();
      }
      _widgetStack->addWidget(resultsListPage);
   }

   UpdateStatus();
}

FridgeClientWindow :: ~FridgeClientWindow()
{
   DeleteConnectionPage();
}

void FridgeClientWindow :: ReturnToDiscoveryRequested()
{
   // Schedule an async callback to do this, to avoid re-entrancy problems
   QTimer::singleShot(0, this, SLOT(ReturnToDiscoveryRequestedAux()));
}

void FridgeClientWindow :: ReturnToDiscoveryRequestedAux()
{
   DeleteConnectionPage();
   UpdateStatus();
}

void FridgeClientWindow :: DeleteConnectionPage()
{
   if (_canvas)     {delete _canvas;     _canvas     = NULL;}
   if (_connection) {delete _connection; _connection = NULL;}
   if (_canvasPage) {delete _canvasPage; _canvasPage = NULL;}  // do this last, as it may be parent of the above
}

void FridgeClientWindow :: SystemItemClicked(QListWidgetItem * item)
{
   DeleteConnectionPage();  // paranoia
   ConnectTo(item->data(Qt::UserRole).toString().toUtf8().constData());
}

void FridgeClientWindow :: ConnectTo(const String & systemName)
{
   status_t ret;
   _connection = new MessageTreeClientConnector(_discoClient.GetCallbackMechanism(), FRIDGE_PROGRAM_SIGNATURE, systemName);
   if (_connection->Start().IsOK(ret))
   {
      _canvasPage = new QWidget;
      {
         QBoxLayout * canvasPageLayout = new QBoxLayout(QBoxLayout::TopToBottom, _canvasPage);

         _canvas = new FridgeClientCanvas(_connection);
         connect(_canvas, SIGNAL(UpdateWindowStatus()), this, SLOT(UpdateStatus()));
         canvasPageLayout->addWidget(_canvas, 1);

         QWidget * buttonsRow = new QWidget;
         {
            QBoxLayout * buttonsRowLayout = new QBoxLayout(QBoxLayout::LeftToRight, buttonsRow);

            buttonsRowLayout->addStretch();

            QPushButton * cloneButton = new QPushButton(tr("Clone Window"));
            connect(cloneButton, SIGNAL(clicked()), this, SLOT(CloneWindow()));
            buttonsRowLayout->addWidget(cloneButton);

            buttonsRowLayout->addStretch();
           
            QPushButton * clearButton = new QPushButton(tr("Clear Magnets"));
            connect(clearButton, SIGNAL(clicked()), this, SLOT(ClearMagnets()));
            buttonsRowLayout->addWidget(clearButton);

            buttonsRowLayout->addStretch();

            QPushButton * disconnectButton = new QPushButton(tr("Disconnect"));
            connect(disconnectButton, SIGNAL(clicked()), this, SLOT(ReturnToDiscoveryRequested()));
            buttonsRowLayout->addWidget(disconnectButton);

            buttonsRowLayout->addStretch();
         }
         canvasPageLayout->addWidget(buttonsRow);
      }
      _widgetStack->addWidget(_canvasPage);
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't start CoreConnectionModule for system [%s]! [%s]\n", systemName(), ret());
      delete _connection;
      _connection = NULL;
   }

   UpdateStatus();
}

void FridgeClientWindow :: ClearMagnets()
{
   if (_canvas) _canvas->ClearMagnets();
}

void FridgeClientWindow :: CloneWindow()
{
   FridgeClientWindow * clone = new FridgeClientWindow(_discoClient.GetCallbackMechanism());
   if (_connection) clone->ConnectTo(_connection->GetSystemNamePattern());
   clone->show();
}

void FridgeClientWindow :: UpdateStatus()
{
   _widgetStack->setCurrentIndex(_connection ? PAGE_MAGNETS : ((_systemsList->count() > 0) ? PAGE_DISCOVERY_LIST : PAGE_DISCOVERY_NO_RESULTS));

   QString windowTitle = tr("Fridge Client");
   if ((_connection)&&(_connection->IsConnected()))
   {
      ZGPeerID peerID;
      const Message * peerInfoMsg = _connection->GetConnectedPeerInfo()();
      if (peerInfoMsg) (void) peerInfoMsg->FindFlat(ZG_DISCOVERY_NAME_PEERID, peerID);

      windowTitle += tr(" -- Connected to %1").arg(_connection->GetSystemNamePattern()());
      if (peerID.IsValid()) windowTitle += tr(" (Peer ID %1)").arg(peerID.ToString()());
   }

   setWindowTitle(windowTitle);

   const bool isPinging  = _discoClient.IsActive();
   const bool shouldPing = (_connection == NULL);
   if (shouldPing != isPinging)
   {
      if (shouldPing)
      {
         status_t ret;
         if (_discoClient.Start().IsError(ret)) LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't start SystemDiscoveryClient! [%s]\n", ret());
      }
      else _discoClient.Stop();
   }
}

void FridgeClientWindow :: DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo)
{
   QString qsn = systemName();

   QListWidgetItem * lwi = NULL;
   for (int i=0; i<_systemsList->count(); i++)
   {
      QListWidgetItem * next = _systemsList->item(i);
      if (next->data(Qt::UserRole) == qsn)
      {
         lwi = next;
         break;
      }
   }

   if (optSystemInfo())
   {
      if (lwi == NULL)
      {
         lwi = new QListWidgetItem(qsn, _systemsList);
         lwi->setData(Qt::UserRole, qsn);
      }
      const int numServers = optSystemInfo()->GetNumValuesInName(ZG_DISCOVERY_NAME_PEERINFO, B_MESSAGE_TYPE);
      lwi->setText(tr("%1 (%2 %3)").arg(qsn).arg(numServers).arg((numServers==1)?tr("server"):tr("servers")));
   }
   else delete lwi;

   UpdateStatus();
}

}; // end namespace fridge

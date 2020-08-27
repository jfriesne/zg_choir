#include <QApplication>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>

#include "dataio/FileDataIO.h"
#include "FridgeChatView.h"
#include "FridgeClientCanvas.h"
#include "FridgeClientWindow.h"
#include "zg/ZGPeerID.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for ZG_DISCOVERY_NAME_PEERINFO

namespace fridge {

static const char * _defaultNamesList[] = {
#include "common_names_list.txt"
};

static const char * GetRandomBabyName(unsigned * seed)
{
   return _defaultNamesList[rand_r(seed)%ARRAYITEMS(_defaultNamesList)];
}

class FridgeClientCanvas;

FridgeClientWindow :: FridgeClientWindow(ICallbackMechanism * callbackMechanism) 
   : IDiscoveryNotificationTarget(NULL)  // can't pass in &_discoClient here, it isn't constructed yet!
   , ITreeGatewaySubscriber(NULL)
   , _discoClient(callbackMechanism, FRIDGE_PROGRAM_SIGNATURE)
   , _splitter(NULL)
   , _connection(NULL)
   , _canvas(NULL)
   , _chatView(NULL)
   , _clearMagnetsButton(NULL)
   , _clearChatButton(NULL)
   , _undoButton(NULL)
   , _redoButton(NULL)
   , _updateStatusPending(false)
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
         rlpLayout->setSpacing(3);
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

   ScheduleUpdateStatus();
}

FridgeClientWindow :: ~FridgeClientWindow()
{
   DeleteConnectionPage();
}

void FridgeClientWindow :: ScheduleUpdateStatus()
{
   if (_updateStatusPending == false)
   {
      _updateStatusPending = true;
      QTimer::singleShot(0, this, SLOT(UpdateStatus()));
   }
}

void FridgeClientWindow :: ReturnToDiscoveryRequested()
{
   // Schedule an async callback to do this, to avoid re-entrancy problems
   QTimer::singleShot(0, this, SLOT(ReturnToDiscoveryRequestedAux()));
}

void FridgeClientWindow :: ReturnToDiscoveryRequestedAux()
{
   DeleteConnectionPage();
   ScheduleUpdateStatus();
}

void FridgeClientWindow :: DeleteConnectionPage()
{
   _undoStackTopPath.Clear();
   _undoStackTop.Reset();
   _undoButton = NULL;

   _redoStackTopPath.Clear();
   _redoStackTop.Reset();
   _redoButton = NULL;

   _clearMagnetsButton = NULL;
   _clearChatButton    = NULL;

   if (_canvas)     {delete _canvas;     _canvas     = NULL;}
   if (_chatView)   {delete _chatView;   _chatView   = NULL;}
   if (_connection) {delete _connection; _connection = NULL;}
   if (_splitter)   {delete _splitter;   _splitter   = NULL;}  // do this last, as it may be parent of the above
}

void FridgeClientWindow :: SystemItemClicked(QListWidgetItem * item)
{
   DeleteConnectionPage();  // paranoia
   ConnectTo(item->data(Qt::UserRole).toString().toUtf8().constData());
}

void FridgeClientWindow :: keyPressEvent(QKeyEvent * e)
{
   if (_chatView)
   {
      _chatView->AcceptKeyPressEventFromWindow(e);
      e->accept();
   }
   else QMainWindow::keyPressEvent(e);
}

void FridgeClientWindow :: ConnectTo(const String & systemName)
{
   status_t ret;
   _connection = new MessageTreeClientConnector(_discoClient.GetCallbackMechanism());
   if (_connection->Start(FRIDGE_PROGRAM_SIGNATURE, systemName).IsOK(ret))
   {
      SetGateway(_connection);

      _splitter = new QSplitter(Qt::Vertical);
      {
         QWidget * topPart = new QWidget;
         {
            QBoxLayout * topPartLayout = new QBoxLayout(QBoxLayout::TopToBottom, topPart);
            topPartLayout->setMargin(3);
            topPartLayout->setSpacing(2);

            QWidget * topButtonsRow = new QWidget;
            {
               QBoxLayout * tbrLayout = new QBoxLayout(QBoxLayout::LeftToRight, topButtonsRow);
               tbrLayout->setMargin(0);

               _undoButton = new QPushButton(tr("Undo"));
               connect(_undoButton, SIGNAL(clicked()), this, SLOT(Undo()));
               tbrLayout->addWidget(_undoButton);

               tbrLayout->addStretch();

               _redoButton = new QPushButton(tr("Redo"));
               connect(_redoButton, SIGNAL(clicked()), this, SLOT(Redo()));
               tbrLayout->addWidget(_redoButton);
            }
            topPartLayout->addWidget(topButtonsRow);

            _canvas = new FridgeClientCanvas(_connection);
            connect(_canvas, SIGNAL(UpdateWindowStatus()), this, SLOT(ScheduleUpdateStatus()));
            topPartLayout->addWidget(_canvas, 1);
 
            QWidget * bottomButtonsRow = new QWidget;
            {
               QBoxLayout * bbrLayout = new QBoxLayout(QBoxLayout::LeftToRight, bottomButtonsRow);
               bbrLayout->setMargin(0);

               bbrLayout->addStretch();
   
               QPushButton * cloneButton = new QPushButton(tr("Clone Window"));
               connect(cloneButton, SIGNAL(clicked()), this, SLOT(CloneWindow()));
               bbrLayout->addWidget(cloneButton);
   
               bbrLayout->addStretch();
              
               _clearMagnetsButton = new QPushButton(tr("Clear Magnets"));
               connect(_clearMagnetsButton, SIGNAL(clicked()), this, SLOT(ClearMagnets()));
               bbrLayout->addWidget(_clearMagnetsButton);

               bbrLayout->addStretch();

               _clearChatButton = new QPushButton(tr("Clear Chat"));
               connect(_clearChatButton, SIGNAL(clicked()), this, SLOT(ClearChat()));
               bbrLayout->addWidget(_clearChatButton);

               bbrLayout->addStretch();
               const QChar ellipses = QChar(0x26, 0x20);

               QPushButton * openProjectButton = new QPushButton(tr("Open Project")+ellipses);
               connect(openProjectButton, SIGNAL(clicked()), this, SLOT(OpenProject()));
               bbrLayout->addWidget(openProjectButton);

               bbrLayout->addStretch();
   
               QPushButton * saveProjectButton = new QPushButton(tr("Save Project")+ellipses);
               connect(saveProjectButton, SIGNAL(clicked()), this, SLOT(SaveProject()));
               bbrLayout->addWidget(saveProjectButton);

               bbrLayout->addStretch();
   
               QPushButton * disconnectButton = new QPushButton(tr("Disconnect"));
               connect(disconnectButton, SIGNAL(clicked()), this, SLOT(ReturnToDiscoveryRequested()));
               bbrLayout->addWidget(disconnectButton);
   
               bbrLayout->addStretch();
            }
            topPartLayout->addWidget(bottomButtonsRow);
         }
         _splitter->addWidget(topPart);
   
         unsigned seed = time(NULL);
         _chatView = new FridgeChatView(_connection, GetRandomBabyName(&seed));
         _chatView->setMinimumHeight(100);
         _splitter->addWidget(_chatView);
      }
      _widgetStack->addWidget(_splitter);
      _splitter->setStretchFactor(0, 2);

      _undoStackTopPath = String("project/undo/%1").Arg(_connection->GetUndoKey());  // so we can update the label of the Undo button appropriately
      AddTreeSubscription(_undoStackTopPath);

      _redoStackTopPath = String("project/redo/%1").Arg(_connection->GetUndoKey());  // so we can update the label of the Redo button appropriately
      AddTreeSubscription(_redoStackTopPath);
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't start CoreConnectionModule for system [%s]! [%s]\n", systemName(), ret());
      delete _connection;
      _connection = NULL;
   }

   ScheduleUpdateStatus();
}

void FridgeClientWindow :: ClearMagnets()
{
   if (_canvas) _canvas->ClearMagnets();
}

void FridgeClientWindow :: ClearChat()
{
   if (_chatView) _chatView->ClearChat();
}

void FridgeClientWindow :: CloneWindow()
{
   FridgeClientWindow * clone = new FridgeClientWindow(_discoClient.GetCallbackMechanism());
   if (_connection) clone->ConnectTo(_connection->GetSystemNamePattern());
   clone->show();
}

void FridgeClientWindow :: UpdateStatus()
{
   _updateStatusPending = false;
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

   UpdateUndoRedoButton(_undoButton, _undoStackTop, tr("Undo"));
   UpdateUndoRedoButton(_redoButton, _redoStackTop, tr("Redo"));

   if (_clearMagnetsButton) _clearMagnetsButton->setEnabled(_canvas->HasMagnets());
}

void FridgeClientWindow :: UpdateUndoRedoButton(QPushButton * button, const MessageRef & msgRef, const QString & verb)
{
   if (button)
   {
      button->setEnabled(msgRef() != NULL);
      if (msgRef()) button->setText(tr("%1 %2").arg(verb).arg(msgRef()->GetCstr("lab", "???")));
               else button->setText(verb);
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

   ScheduleUpdateStatus();
}

void FridgeClientWindow :: SaveProject()
{
   // First, download the current state of the magnets.  We'll show the file dialog when the requested subtree arrives.
   Queue<String> paths;
   if (paths.AddTail("project/magnets").IsOK()) (void) RequestTreeNodeSubtrees(paths, Queue<ConstQueryFilterRef>(), "save_project");
}

#ifdef WIN32
static inline QString LocalFromQAux(const QString & qs)
{
   QString tmp = qs;
   return tmp.replace('/', '\\');  // FogBugz #5816:  encodeName() doesn't convert slashes back, so we have to do it
}
# define LocalFromQ(qs) (QFile::encodeName(LocalFromQAux(qs)).constData())
#else
# define LocalFromQ(qs) (QFile::encodeName(qs).constData())
#endif

void FridgeClientWindow :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   if (tag == "save_project")
   {
      if (subtreeData())
      {
         const QString saveFile = QFileDialog::getSaveFileName(this, tr("Save Magnets Project"), QString(), tr("Magnets File (*.magnets)"));
         if (saveFile.size() > 0)
         {
            FileDataIO fdio(fopen(LocalFromQ(saveFile), "wb"));
            if (fdio.GetFile())
            {
               status_t ret;
               if (subtreeData()->FlattenToDataIO(fdio, false).IsError(ret)) QMessageBox::critical(this, tr("Project download error"), tr("Error writing data to file [%1] [%2]").arg(saveFile).arg(ret()));
            }
            else QMessageBox::critical(this, tr("Project download error"), tr("Error, couldn't write to file [%1]").arg(saveFile));
         }
      }
      else QMessageBox::critical(this, tr("Project download error"), tr("Error, couldn't download magnets project!"));
   }
}

void FridgeClientWindow :: TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg)
{
   if (nodePath == _undoStackTopPath)
   {
      _undoStackTop = optPayloadMsg;
      ScheduleUpdateStatus();
   }
   else if (nodePath == _redoStackTopPath)
   {
      _redoStackTop = optPayloadMsg;
      ScheduleUpdateStatus();
   }
}

void FridgeClientWindow :: OpenProject()
{
   const QString openFile = QFileDialog::getOpenFileName(this, tr("Open Magnets Project"), QString(), tr("Magnets File (*.magnets)"));
   if (openFile.size() > 0)
   {
      FileDataIO fdio(fopen(LocalFromQ(openFile), "rb"));
      if (fdio.GetFile())
      {
         status_t ret;
         MessageRef subtreeData = GetMessageFromPool();
         if (subtreeData()->UnflattenFromDataIO(fdio, fdio.GetLength()).IsOK(ret))
         {
            (void) BeginUndoSequence(String("Open Project [%1]").Arg(openFile.toUtf8().constData()));
            if (UploadTreeNodeSubtree("project/magnets", subtreeData).IsError(ret)) QMessageBox::critical(this, tr("Project open error"), tr("Error uploading data from file [%1] [%2]").arg(openFile).arg(ret()));
            (void) EndUndoSequence();
         }
         else QMessageBox::critical(this, tr("Project open error"), tr("Error reading data from file [%1] [%2]").arg(openFile).arg(ret()));
      }
      else QMessageBox::critical(this, tr("Project open error"), tr("Error, couldn't open file [%1] for reading").arg(openFile));
   }
}

void FridgeClientWindow :: Undo()
{
   (void) RequestUndo();
}

void FridgeClientWindow :: Redo()
{
   (void) RequestRedo();
}

}; // end namespace fridge

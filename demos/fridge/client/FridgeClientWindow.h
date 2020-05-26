#ifndef FridgeClientWindow_h
#define FridgeClientWindow_h

#include <QMainWindow>

#include "common/FridgeNameSpace.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"
#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "util/String.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSplitter;
class QStackedWidget;

namespace fridge {

class FridgeChatView;
class FridgeClientCanvas;

/** This is a demo client that allows the user to mess around with refrigerator-magnets stored on a FridgeServer system */
class FridgeClientWindow : public QMainWindow, public IDiscoveryNotificationTarget, public ITreeGatewaySubscriber
{
Q_OBJECT

public:
   /** Constructor
     * @param callbackMechanism the ICallbackMechanim used by the ZG classes to inject method-callbacks into the GUI thread.
     */
   FridgeClientWindow(ICallbackMechanism * callbackMechanism);

   /** Destructor */
   virtual ~FridgeClientWindow();

   virtual void DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo);

   virtual void keyPressEvent(QKeyEvent * ke);

protected:
   // ITreeGatewaySubscriber API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);

private slots:
   void CloneWindow();
   void UpdateStatus();
   void ScheduleUpdateStatus();
   void ReturnToDiscoveryRequested();
   void ReturnToDiscoveryRequestedAux();
   void SystemItemClicked(QListWidgetItem * item);
   void ClearMagnets();
   void OpenProject();
   void SaveProject();
   void Undo();
   void Redo();

private:
   void UpdateGUI();
   void DeleteConnectionPage();
   void ConnectTo(const String & systemName);
   void UpdateUndoRedoButton(QPushButton * button, const MessageRef & msgRef, const QString & verb);

   enum {
      PAGE_DISCOVERY_NO_RESULTS = 0,
      PAGE_DISCOVERY_LIST,
      PAGE_MAGNETS,
      NUM_PAGES
   };

   SystemDiscoveryClient _discoClient;

   QStackedWidget * _widgetStack;
   QListWidget * _systemsList;

   // The following widgets get created when the user
   // chooses a system, and destroyed when the user disconnects from it
   QSplitter * _splitter;
   MessageTreeClientConnector * _connection;
   FridgeClientCanvas * _canvas;
   FridgeChatView * _chatView;

   QPushButton * _clearButton;
   QPushButton * _undoButton;
   QPushButton * _redoButton;

   String _undoStackTopPath;  // e.g. "project/undo/<KEY>/top", only when we're connected
   MessageRef _undoStackTop;  // current Message held by the server at (_undoStackTopPath)

   String _redoStackTopPath;  // e.g. "project/redo/<KEY>/top", only when we're connected
   MessageRef _redoStackTop;  // current Message held by the server at (_redoStackTopPath)

   bool _updateStatusPending;
};

}; // end namespace fridge

#endif

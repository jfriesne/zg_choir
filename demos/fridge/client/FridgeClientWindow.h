#ifndef FridgeClientWindow_h
#define FridgeClientWindow_h

#include <QMainWindow>

#include "common/FridgeNameSpace.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"
#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "util/String.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

namespace fridge {

class FridgeClientCanvas;

/** This is a demo client that allows the user to mess around with refrigerator-magnets stored on a FridgeServer system */
class FridgeClientWindow : public QMainWindow, public IDiscoveryNotificationTarget
{
Q_OBJECT

public:
   /** Constructor */
   FridgeClientWindow(ICallbackMechanism * callbackMechanism);

   /** Destructor */
   virtual ~FridgeClientWindow();

   virtual void DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo);

private slots:
   void CloneWindow();
   void UpdateStatus();
   void ReturnToDiscoveryRequested();
   void ReturnToDiscoveryRequestedAux();
   void SystemItemClicked(QListWidgetItem * item);
   void ClearMagnets();

private:
   void UpdateGUI();
   void DeleteConnectionPage();
   void ConnectTo(const String & systemName);

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
   QWidget * _canvasPage;
   MessageTreeClientConnector * _connection;
   FridgeClientCanvas * _canvas;
};

}; // end namespace fridge

#endif

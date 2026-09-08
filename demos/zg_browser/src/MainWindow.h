#ifndef MainWindow_h
#define MainWindow_h

#include <QMainWindow>

#include "platform/qt/QPostEventCallbackMechanism.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

#include "ZGBrowserNameSpace.h"

class QStackedWidget;

namespace zg_browser {

class DiscoveryWidget;
class BrowserWidget;

/** The app's one window:  the systems list, and the browser for the chosen system. */
class MainWindow MUSCLE_FINAL_CLASS : public QMainWindow
{
Q_OBJECT

public:
   /** @param optSystemName if non-empty, open the browser for this system straight
     *                       away instead of showing the systems list first.
     */
   explicit MainWindow(const String & optSystemName = String(), QWidget * parent = NULL);
   ~MainWindow() override;

private slots:
   void showBrowserFor(const String & signaturePattern, const String & systemName);
   void showDiscovery();

private:
   // Declaration order matters:  the callback mechanism has to outlive everything
   // that posts callbacks through it, and the discovery client has to outlive the
   // widget that registers with it.
   QPostEventCallbackMechanism _callbackMechanism;
   SystemDiscoveryClient _discoveryClient;

   QStackedWidget * _stack;
   DiscoveryWidget * _discoveryView;
   BrowserWidget * _browserView = NULL;
};

}  // end zg_browser namespace

#endif  // MainWindow_h

#pragma once

#include <QMainWindow>

#include "MuscleQt.h"

#include "platform/qt/QPostEventCallbackMechanism.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

class BrowserWidget;
class DiscoveryWidget;
class QStackedWidget;

/** The app's one window:  the systems list, and the browser for the chosen system. */
class MainWindow final : public QMainWindow
{
   Q_OBJECT

public:
   /** @param optSystemName if non-empty, open the browser for this system straight
     *                       away instead of showing the systems list first.
     */
   explicit MainWindow(const muscle::String & optSystemName = muscle::String(), QWidget * parent = NULL);
   ~MainWindow() override;

private slots:
   void showBrowserFor(const muscle::String & signaturePattern, const muscle::String & systemName);
   void showDiscovery();

private:
   // Declaration order matters:  the callback mechanism has to outlive everything
   // that posts callbacks through it, and the discovery client has to outlive the
   // widget that registers with it.
   muscle::QPostEventCallbackMechanism _callbackMechanism;
   zg::SystemDiscoveryClient _discoveryClient;

   QStackedWidget * _stack;
   DiscoveryWidget * _discoveryView;
   BrowserWidget * _browserView = NULL;
};

#include "MainWindow.h"

#include <QStackedWidget>

#include "BrowserWidget.h"
#include "DiscoveryWidget.h"

using namespace muscle;

MainWindow :: MainWindow(const String & optSystemName, QWidget * parent)
   : QMainWindow(parent)
   , _discoveryClient(&_callbackMechanism, "*")   // "*" == every kind of ZG server
{
   setWindowTitle(tr("ZG Browser"));

   _stack = new QStackedWidget;
   setCentralWidget(_stack);

   _discoveryView = new DiscoveryWidget(_discoveryClient);
   connect(_discoveryView, &DiscoveryWidget::systemChosen, this, &MainWindow::showBrowserFor);
   _stack->addWidget(_discoveryView);

   status_t ret;
   if (_discoveryClient.Start().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't start the SystemDiscoveryClient [%s]\n", ret());
   }

   setMinimumSize(600, 400);
   resize(1100, 700);

   if (optSystemName.HasChars()) showBrowserFor("*", optSystemName);
}

MainWindow :: ~MainWindow()
{
   // Deleting the browser stops its connector thread and drops its subscriptions;
   // do that before the discovery client (and the callback mechanism) go away.
   delete _browserView;
   _browserView = NULL;

   _discoveryClient.Stop();
}

void MainWindow :: showBrowserFor(const String & signaturePattern, const String & systemName)
{
   showDiscovery();   // drop any previous browser first

   _browserView = new BrowserWidget(_callbackMechanism, signaturePattern, systemName);
   connect(_browserView, &BrowserWidget::backRequested, this, &MainWindow::showDiscovery);
   _stack->addWidget(_browserView);
   _stack->setCurrentWidget(_browserView);
}

void MainWindow :: showDiscovery()
{
   _stack->setCurrentWidget(_discoveryView);

   // Deleting the browser also stops its connector thread and drops its subscriptions.
   if (_browserView)
   {
      _stack->removeWidget(_browserView);
      delete _browserView;
      _browserView = NULL;
   }

   _discoveryView->setFocus();
}

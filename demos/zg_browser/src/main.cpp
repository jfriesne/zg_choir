#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

#include "system/SetupSystem.h"

#include "MainWindow.h"

using namespace zg_browser;

/** A browser for ZG (zg_choir) systems, in the spirit of muscle's qt_muscled_browser. */
int main(int argc, char ** argv)
{
   // MUSCLE requires this object to exist for as long as any muscle/zg code runs,
   // so it is declared first and destroyed last.
   CompleteSetupSystem setupSystem;

   QApplication app(argc, argv);
   QApplication::setApplicationName("ZG Browser");
   QApplication::setApplicationVersion("0.1.0");

   QCommandLineParser parser;
   parser.setApplicationDescription(QCoreApplication::translate("main", "Browses the database of a ZG system on the local network."));
   parser.addHelpOption();
   parser.addVersionOption();

   const QCommandLineOption systemNameOption(QStringList() << "s" << "system-name",
      QCoreApplication::translate("main", "Skip the systems list and browse this system straight away (wildcards allowed)."),
      QCoreApplication::translate("main", "system-name"));
   parser.addOption(systemNameOption);
   parser.process(app);

   MainWindow window(FromQ(parser.value(systemNameOption)));
   window.show();

   return QApplication::exec();
}

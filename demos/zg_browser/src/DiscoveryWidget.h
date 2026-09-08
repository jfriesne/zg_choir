#pragma once

#include <vector>

#include <QWidget>

#include "MuscleQt.h"

#include "util/Hashtable.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/** The list of ZG systems currently visible on the local network. */
class DiscoveryWidget final : public QWidget,
                              private zg::IDiscoveryNotificationTarget
{
   Q_OBJECT

public:
   explicit DiscoveryWidget(zg::SystemDiscoveryClient & discoveryClient, QWidget * parent = NULL);
   ~DiscoveryWidget() override;

   /** Keeps the "listening..." placeholder covering the list box. */
   bool eventFilter(QObject * watched, QEvent * event) override;

signals:
   /** Emitted with (signaturePattern, systemName) when the user picks a system to browse. */
   void systemChosen(const muscle::String & signaturePattern, const muscle::String & systemName);

   // IDiscoveryNotificationTarget
   void DiscoveryUpdate(const muscle::String & systemName, const muscle::MessageRef & optSystemInfo) override;

private slots:
   void chooseRow(int rowNumber);
   void connectToTypedSystemName();

private:

   void rebuildRows();

   struct SystemRow
   {
      muscle::String _systemName;
      muscle::String _signature;
      QString _detail;
   };

   muscle::Hashtable<muscle::String, muscle::MessageRef> _systems;
   std::vector<SystemRow> _rows;

   QListWidget * _listBox;
   QLabel * _emptyLabel;
   QLineEdit * _manualName;
   QPushButton * _manualConnect;
};

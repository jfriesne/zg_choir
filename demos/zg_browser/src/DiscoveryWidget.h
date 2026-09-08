#ifndef DiscoveryWidget_h
#define DiscoveryWidget_h

#include <QWidget>

#include "util/Hashtable.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

#include "ZGBrowserNameSpace.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace zg_browser {

/** The list of ZG systems currently visible on the local network. */
class DiscoveryWidget MUSCLE_FINAL_CLASS : public QWidget, private IDiscoveryNotificationTarget
{
Q_OBJECT

public:
   explicit DiscoveryWidget(SystemDiscoveryClient & discoveryClient, QWidget * parent = NULL);
   ~DiscoveryWidget() override;

   /** Keeps the "listening..." placeholder covering the list box. */
   bool eventFilter(QObject * watched, QEvent * event) override;

   // IDiscoveryNotificationTarget
   void DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo) override;

signals:
   /** Emitted with (signaturePattern, systemName) when the user picks a system to browse. */
   void systemChosen(const String & signaturePattern, const String & systemName);

private slots:
   void chooseRow(int rowNumber);
   void connectToTypedSystemName();

private:

   void rebuildRows();

   struct SystemRow
   {
   public:
      String _systemName;
      String _signature;
      QString _detail;

      bool operator == (const SystemRow & rhs) const {return (Compare(rhs) == 0);}
      bool operator  < (const SystemRow & rhs) const {return (Compare(rhs)  < 0);}

      int Compare(const SystemRow & rhs) const
      {
         int r = _systemName.NumericAwareCompareToIgnoreCase(rhs._systemName);
         if (r) return r;

         r = _signature.NumericAwareCompareTo(rhs._signature);
         if (r) return r;

         return _detail.compare(rhs._detail);
      }
   };

   Hashtable<String, MessageRef> _systems;
   Queue<SystemRow> _rows;

   QListWidget * _listBox;
   QLabel * _emptyLabel;
   QLineEdit * _manualName;
   QPushButton * _manualConnect;
};

}  // end namespace zg_browser

#endif  // DiscoveryWidget_h

#ifndef BrowserWidget_h
#define BrowserWidget_h

#include <QWidget>

#include "util/Hashtable.h"
#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

#include "ZGBrowserNameSpace.h"

class QLabel;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;

namespace zg_browser {

class MessagePanel;
class NodeTreeItem;

/** Browses the database of one ZG system.
  *
  * The MessageTreeClientConnector does the discovery, TCP connection and
  * automatic reconnection for us; we just drive the tree on top of it.  A node
  * of the tree is subscribed to when it is opened and unsubscribed from when it
  * is closed, so the client only ever holds the part of the database that is
  * actually on screen -- and that part is always live.
  */
class BrowserWidget MUSCLE_FINAL_CLASS : public QWidget, private ITreeGatewaySubscriber
{
Q_OBJECT

public:
   /** @param callbackMechanism marshals the network thread's callbacks onto the GUI thread
     * @param signaturePattern the kind of ZG server to connect to (may be wildcarded, eg "*")
     * @param systemNamePattern the ZG system to connect to (may be wildcarded)
     */
   BrowserWidget(ICallbackMechanism & callbackMechanism,
                 const String & signaturePattern,
                 const String & systemNamePattern,
                 QWidget * parent = NULL);

   ~BrowserWidget() override;

   // ITreeGatewaySubscriber
   void TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & optOpTag) override;
   void TreeGatewayConnectionStateChanged() override;

signals:
   /** Emitted when the user wants to go back to the discovery list. */
   void backRequested();

protected:
   void resizeEvent(QResizeEvent * event) override;
   void showEvent(QShowEvent * event) override;
   void CallbackBatchEnds() override;

private slots:
   void nodeItemExpanded(QTreeWidgetItem * item);
   void nodeItemCollapsed(QTreeWidgetItem * item);
   void nodeItemSelected(QTreeWidgetItem * item);

private:
   void subscribeToChildrenOf(const String & nodePath, NodeTreeItem & item);
   void unsubscribeFromChildrenOf(const String & nodePath, NodeTreeItem & item);
   void unsubscribeFromDescendantsOf(const String & nodePath, bool includeSelf);
   void forgetCachedDataUnder(const String & nodePath, bool includeSelf);

   NodeTreeItem * findItemForPath(const String & nodePath) const;
   NodeTreeItem * createChildItem(NodeTreeItem & parentItem, const String & childName);

   void handleNodeAddedOrUpdated(const String & nodePath);
   void handleNodeRemoved(const String & nodePath);
   void refreshMessagePanel();
   void updateConnectionStateUI();
   void updateSummaryFor(NodeTreeItem & item);
   void layOutOverlay();

   const String _systemName;

   MessageTreeClientConnector _connector;

   // The part of the server's database we're currently holding, by session-relative path
   Hashtable<String, ConstMessageRef> _pathToMessage;

   // The subscription-strings (eg "srv/*") we currently hold, one per open tree node
   Hashtable<String, Void> _subscriptions;

   String _selectedPath;
   bool _hasSelection = false;
   bool _messagePanelNeedsRefresh = false;
   bool _wasConnected = false;
   bool _hasEverConnected = false;

   QLabel * _statusLabel;
   QTreeWidget * _treeView;
   NodeTreeItem * _rootItem;
   MessagePanel * _messagePanel;
   QSplitter * _splitter;

   /** Semi-transparent "we're not connected right now" cover over the splitter. */
   QLabel * _overlay;
};

}  // end namespace zg_browser

#endif  // BrowserWidget_h

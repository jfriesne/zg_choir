#pragma once

#include <QWidget>

#include "MuscleQt.h"

#include "util/Hashtable.h"
#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

class MessagePanel;
class NodeTreeItem;
class QLabel;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;

/** Browses the database of one ZG system.
  *
  * The MessageTreeClientConnector does the discovery, TCP connection and
  * automatic reconnection for us; we just drive the tree on top of it.  A node
  * of the tree is subscribed to when it is opened and unsubscribed from when it
  * is closed, so the client only ever holds the part of the database that is
  * actually on screen -- and that part is always live.
  */
class BrowserWidget final : public QWidget,
                            private zg::ITreeGatewaySubscriber
{
   Q_OBJECT

public:
   /** @param callbackMechanism marshals the network thread's callbacks onto the GUI thread
     * @param signaturePattern the kind of ZG server to connect to (may be wildcarded, eg "*")
     * @param systemNamePattern the ZG system to connect to (may be wildcarded)
     */
   BrowserWidget(muscle::ICallbackMechanism & callbackMechanism,
                 const muscle::String & signaturePattern,
                 const muscle::String & systemNamePattern,
                 QWidget * parent = NULL);

   ~BrowserWidget() override;

signals:
   /** Emitted when the user wants to go back to the discovery list. */
   void backRequested();

protected:
   void resizeEvent(QResizeEvent * event) override;
   void showEvent(QShowEvent * event) override;

private slots:
   void nodeItemExpanded(QTreeWidgetItem * item);
   void nodeItemCollapsed(QTreeWidgetItem * item);
   void nodeItemSelected(QTreeWidgetItem * item);

private:
   // ITreeGatewaySubscriber
   void TreeNodeUpdated(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayloadMsg, const muscle::String & optOpTag) override;
   void TreeGatewayConnectionStateChanged() override;
   void CallbackBatchEnds() override;

   void subscribeToChildrenOf(const muscle::String & nodePath, NodeTreeItem & item);
   void unsubscribeFromChildrenOf(const muscle::String & nodePath, NodeTreeItem & item);
   void unsubscribeFromDescendantsOf(const muscle::String & nodePath, bool includeSelf);
   void forgetCachedDataUnder(const muscle::String & nodePath, bool includeSelf);

   NodeTreeItem * findItemForPath(const muscle::String & nodePath) const;
   NodeTreeItem * createChildItem(NodeTreeItem & parentItem, const muscle::String & childName);

   void handleNodeAddedOrUpdated(const muscle::String & nodePath);
   void handleNodeRemoved(const muscle::String & nodePath);
   void refreshMessagePanel();
   void updateConnectionStateUI();
   void updateSummaryFor(NodeTreeItem & item);
   void layOutOverlay();

   const muscle::String _systemName;

   zg::MessageTreeClientConnector _connector;

   // The part of the server's database we're currently holding, by session-relative path
   muscle::Hashtable<muscle::String, muscle::ConstMessageRef> _pathToMessage;

   // The subscription-strings (eg "srv/*") we currently hold, one per open tree node
   muscle::Hashtable<muscle::String, muscle::Void> _subscriptions;

   muscle::String _selectedPath;
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

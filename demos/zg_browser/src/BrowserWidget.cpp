#include "BrowserWidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "MessagePanel.h"
#include "NodeTreeItem.h"
#include "Theme.h"

#include "zg/discovery/common/DiscoveryUtilityFunctions.h"   // for ZG_DISCOVERY_NAME_*

using namespace muscle;

BrowserWidget :: BrowserWidget(ICallbackMechanism & callbackMechanism,
                               const String & signaturePattern,
                               const String & systemNamePattern,
                               QWidget * parent)
   : QWidget(parent)
   , zg::ITreeGatewaySubscriber(NULL)   // our gateway isn't constructed yet; we register below
   , _systemName(systemNamePattern)
   , _connector(&callbackMechanism)
{
   SetGateway(&_connector);

   QVBoxLayout * layout = new QVBoxLayout(this);
   layout->setContentsMargins(0, 0, 0, 0);
   layout->setSpacing(0);

   // ---- header ----------------------------------------------------------
   QWidget * header = new QWidget;
   header->setFixedHeight(38);
   header->setStyleSheet(QString("QWidget { background: %1; border-bottom: 1px solid %2; }")
                            .arg(zgb::theme::header.name(), zgb::theme::border.name()));
   {
      QHBoxLayout * headerLayout = new QHBoxLayout(header);
      headerLayout->setContentsMargins(6, 5, 6, 5);
      headerLayout->setSpacing(10);

      QPushButton * backButton = new QPushButton(QString::fromUtf8("← Systems"));
      connect(backButton, &QPushButton::clicked, this, &BrowserWidget::backRequested);
      headerLayout->addWidget(backButton);

      QLabel * titleLabel = new QLabel(zgb::toQt(systemNamePattern));
      {
         QFont f = titleLabel->font();
         f.setBold(true);
         f.setPointSizeF(f.pointSizeF() + 2.0);
         titleLabel->setFont(f);
      }
      headerLayout->addWidget(titleLabel, 1);

      _statusLabel = new QLabel;
      _statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      headerLayout->addWidget(_statusLabel);
   }
   layout->addWidget(header);

   // ---- body ------------------------------------------------------------
   _splitter = new QSplitter(Qt::Horizontal);
   _splitter->setHandleWidth(7);
   {
      _treeView = new QTreeWidget;
      _treeView->setColumnCount(2);
      _treeView->setHeaderLabels(QStringList() << tr("Node") << QString());
      _treeView->header()->setStretchLastSection(false);
      _treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
      _treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
      _treeView->setIndentation(16);
      _treeView->setSelectionMode(QAbstractItemView::SingleSelection);
      _treeView->setUniformRowHeights(true);
      connect(_treeView, &QTreeWidget::itemExpanded,  this, &BrowserWidget::nodeItemExpanded);
      connect(_treeView, &QTreeWidget::itemCollapsed, this, &BrowserWidget::nodeItemCollapsed);
      connect(_treeView, &QTreeWidget::currentItemChanged, this,
              [this](QTreeWidgetItem * cur, QTreeWidgetItem *) {nodeItemSelected(cur);});
      _splitter->addWidget(_treeView);

      _messagePanel = new MessagePanel;
      _splitter->addWidget(_messagePanel);

      _treeView->setMinimumWidth(140);
      _messagePanel->setMinimumWidth(200);
      _splitter->setStretchFactor(0, 0);
      _splitter->setStretchFactor(1, 1);
      _splitter->setSizes(QList<int>() << 280 << 620);
   }
   layout->addWidget(_splitter, 1);

   // The overlay covers the body only:  the header (and with it the "back to
   // the systems list" button) stays usable while we're disconnected.
   _overlay = new QLabel(this);
   _overlay->setAlignment(Qt::AlignCenter);
   _overlay->setWordWrap(true);
   {
      QFont f = _overlay->font();
      f.setBold(true);
      f.setPointSizeF(f.pointSizeF() + 6.0);
      _overlay->setFont(f);
   }
   _overlay->setStyleSheet(QString("QLabel { background: rgba(%1,%2,%3,%4); color: %5; padding: 30px; }")
                              .arg(zgb::theme::overlay.red())
                              .arg(zgb::theme::overlay.green())
                              .arg(zgb::theme::overlay.blue())
                              .arg(zgb::theme::overlay.alpha())
                              .arg(zgb::theme::text.name()));

   _rootItem = new NodeTreeItem(_treeView);

   // Opening the root subscribes us to the top level of the database.  It is
   // fine to do this before we're connected:  the gateway remembers our
   // subscriptions and (re)sends them whenever a connection is established.
   _rootItem->setExpanded(true);

   status_t ret;
   if (_connector.Start(signaturePattern, systemNamePattern).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't start MessageTreeClientConnector for system [%s] [%s]\n", systemNamePattern(), ret());
   }

   updateConnectionStateUI();
}

BrowserWidget :: ~BrowserWidget()
{
   SetGateway(NULL);       // stop receiving callbacks before anything gets torn down
   _connector.Stop();
}

// ---------------------------------------------------------------------------
//  Subscription bookkeeping
// ---------------------------------------------------------------------------

void BrowserWidget :: subscribeToChildrenOf(const String & nodePath, NodeTreeItem & item)
{
   if (item.isSubscribed()) return;

   const String subPath = zgb::childrenSubscriptionString(nodePath);

   status_t ret;
   if (AddTreeSubscription(subPath).IsOK(ret))
   {
      (void) _subscriptions.PutWithDefault(subPath);
      item.setSubscribed(true);
   }
   else LogTime(MUSCLE_LOG_ERROR, "Couldn't subscribe to [%s] [%s]\n", subPath(), ret());
}

void BrowserWidget :: unsubscribeFromChildrenOf(const String & nodePath, NodeTreeItem & item)
{
   if (item.isSubscribed() == false) return;

   const String subPath = zgb::childrenSubscriptionString(nodePath);
   (void) RemoveTreeSubscription(subPath);
   (void) _subscriptions.Remove(subPath);
   item.setSubscribed(false);
}

void BrowserWidget :: unsubscribeFromDescendantsOf(const String & nodePath, bool includeSelf)
{
   const String ownSubPath = zgb::childrenSubscriptionString(nodePath);
   const String prefix     = nodePath.IsEmpty() ? GetEmptyString() : (nodePath + "/");

   for (HashtableIterator<String, Void> iter(_subscriptions); iter.HasData(); iter++)
   {
      const String subPath = iter.GetKey();   // deliberately a copy; we may remove this entry below
      const bool isSelf = (subPath == ownSubPath);
      if ((isSelf ? includeSelf : subPath.StartsWith(prefix)))
      {
         (void) RemoveTreeSubscription(subPath);
         (void) _subscriptions.Remove(subPath);
      }
   }
}

void BrowserWidget :: forgetCachedDataUnder(const String & nodePath, bool includeSelf)
{
   const String prefix = nodePath.IsEmpty() ? GetEmptyString() : (nodePath + "/");

   for (HashtableIterator<String, ConstMessageRef> iter(_pathToMessage); iter.HasData(); iter++)
   {
      const String path = iter.GetKey();      // deliberately a copy; we may remove this entry below
      if (((includeSelf)&&(path == nodePath))||(path.StartsWith(prefix))) (void) _pathToMessage.Remove(path);
   }
}

// ---------------------------------------------------------------------------
//  Tree-item plumbing
// ---------------------------------------------------------------------------

NodeTreeItem * BrowserWidget :: findItemForPath(const String & nodePath) const
{
   NodeTreeItem * item = _rootItem;
   if (nodePath.IsEmpty()) return item;

   uint32 startAt = 0;
   while(item)
   {
      const int slash = nodePath.IndexOf('/', startAt);
      item = item->getChildByName((slash >= 0) ? nodePath.Substring(startAt, (uint32)slash) : nodePath.Substring(startAt));
      if (slash < 0) break;
      startAt = ((uint32)slash)+1;
   }

   return item;
}

NodeTreeItem * BrowserWidget :: createChildItem(NodeTreeItem & parentItem, const String & childName)
{
   NodeTreeItem * newItem = parentItem.addChildNode(childName);
   updateSummaryFor(*newItem);

   // If this node was open before we lost the connection, re-open it without
   // re-subscribing:  the gateway still holds (and has re-sent) that subscription.
   // Note setSubscribed() must come first -- setExpanded() calls us back through
   // nodeItemExpanded(), which would otherwise re-issue the subscription.
   if (_subscriptions.ContainsKey(zgb::childrenSubscriptionString(newItem->getNodePath())))
   {
      newItem->setSubscribed(true);
      newItem->setExpanded(true);
   }

   return newItem;
}

void BrowserWidget :: updateSummaryFor(NodeTreeItem & item)
{
   const ConstMessageRef * msg = _pathToMessage.Get(item.getNodePath());
   const uint32 numFields = msg ? (*msg)()->GetNumNames() : 0;
   item.setSummary(numFields ? (QString::number(numFields) + (numFields == 1 ? tr(" field") : tr(" fields")))
                             : QString());
}

void BrowserWidget :: nodeItemExpanded(QTreeWidgetItem * item)
{
   NodeTreeItem & node = *static_cast<NodeTreeItem *>(item);
   subscribeToChildrenOf(node.getNodePath(), node);
}

void BrowserWidget :: nodeItemCollapsed(QTreeWidgetItem * item)
{
   NodeTreeItem & node = *static_cast<NodeTreeItem *>(item);
   const String nodePath = node.getNodePath();

   unsubscribeFromChildrenOf(nodePath, node);
   unsubscribeFromDescendantsOf(nodePath, false);
   forgetCachedDataUnder(nodePath, false);
   node.clearChildren();
}

void BrowserWidget :: nodeItemSelected(QTreeWidgetItem * item)
{
   if (item == NULL) return;   // deselection deliberately keeps the last shown Message on screen

   _selectedPath = static_cast<NodeTreeItem *>(item)->getNodePath();
   _hasSelection = true;
   refreshMessagePanel();
}

// ---------------------------------------------------------------------------
//  ITreeGatewaySubscriber callbacks
// ---------------------------------------------------------------------------

void BrowserWidget :: TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & /*optOpTag*/)
{
   if (nodePath.IsEmpty()) return;   // the session-root itself is never shown as a child

   if (optPayloadMsg())
   {
      (void) _pathToMessage.Put(nodePath, optPayloadMsg);
      handleNodeAddedOrUpdated(nodePath);
   }
   else
   {
      (void) _pathToMessage.Remove(nodePath);
      handleNodeRemoved(nodePath);
   }

   if ((_hasSelection)&&(_selectedPath == nodePath)) _messagePanelNeedsRefresh = true;
   if (IsInCallbackBatch() == false) CallbackBatchEnds();
}

void BrowserWidget :: handleNodeAddedOrUpdated(const String & nodePath)
{
   NodeTreeItem * parentItem = findItemForPath(zgb::parentPathOf(nodePath));
   if ((parentItem == NULL)||(parentItem->isExpanded() == false)) return;   // we're not showing this part of the tree

   const String childName = zgb::leafNameOf(nodePath);
   NodeTreeItem * item = parentItem->getChildByName(childName);
   if (item == NULL) (void) createChildItem(*parentItem, childName);
              else updateSummaryFor(*item);
}

void BrowserWidget :: handleNodeRemoved(const String & nodePath)
{
   forgetCachedDataUnder(nodePath, false);
   unsubscribeFromDescendantsOf(nodePath, true);

   NodeTreeItem * parentItem = findItemForPath(zgb::parentPathOf(nodePath));
   if (parentItem) parentItem->removeChildNode(zgb::leafNameOf(nodePath));
}

void BrowserWidget :: CallbackBatchEnds()
{
   if (_messagePanelNeedsRefresh)
   {
      _messagePanelNeedsRefresh = false;
      refreshMessagePanel();
   }
}

void BrowserWidget :: TreeGatewayConnectionStateChanged()
{
   const bool isConnected = IsTreeGatewayConnected();
   if (isConnected == _wasConnected) return;
   _wasConnected = isConnected;
   if (isConnected) _hasEverConnected = true;

   if (isConnected == false)
   {
      // Our subscriptions are kept (the gateway re-sends them when the
      // connection comes back), but the data we cached for them is now stale,
      // so drop it and let the fresh subscription-results rebuild the tree.
      _pathToMessage.Clear();
      if (_rootItem) _rootItem->clearChildren();
      _hasSelection = false;
      _selectedPath.Clear();
      _messagePanel->clear();
   }

   updateConnectionStateUI();
}

// ---------------------------------------------------------------------------
//  UI
// ---------------------------------------------------------------------------

void BrowserWidget :: refreshMessagePanel()
{
   if (_hasSelection == false) {_messagePanel->clear(); return;}

   const ConstMessageRef * msg = _pathToMessage.Get(_selectedPath);
   _messagePanel->showNode(_selectedPath, msg ? *msg : ConstMessageRef());
}

void BrowserWidget :: updateConnectionStateUI()
{
   const bool isConnected = IsTreeGatewayConnected();

   QString status;
   if (isConnected)
   {
      const MessageRef peerInfo = _connector.GetConnectedPeerInfo();
      const String source = peerInfo() ? peerInfo()->GetString(ZG_DISCOVERY_NAME_SOURCE) : GetEmptyString();
      const IPAddressAndPort sourceIAP(source, 0, false);   // no DNS lookups; we're on the GUI thread
      const String host = sourceIAP.GetIPAddress().IsValid() ? sourceIAP.ToString(false) : source;
      status = host.IsEmpty() ? tr("Connected") : tr("Connected to %1").arg(zgb::toQt(host));
   }
   else status = tr("Not connected");

   _statusLabel->setStyleSheet(QString("QLabel { color: %1; }")
                                  .arg((isConnected ? zgb::theme::connected : zgb::theme::disconnected).name()));
   _statusLabel->setText(status);

   if (isConnected == false)
   {
      _overlay->setText(_hasEverConnected
         ? tr("Disconnected from \"%1\"\n\nReconnecting automatically as soon as the system comes back...").arg(zgb::toQt(_systemName))
         : tr("Looking for \"%1\" on the local network...").arg(zgb::toQt(_systemName)));
   }
   _overlay->setVisible(isConnected == false);
   layOutOverlay();
}

void BrowserWidget :: layOutOverlay()
{
   if (_overlay->isVisible())
   {
      // Cover the body only:  the header (and with it the "back to the systems
      // list" button) stays usable while we're disconnected.
      _overlay->setGeometry(QRect(_splitter->mapTo(this, QPoint(0, 0)), _splitter->size()));
      _overlay->raise();
   }
}

void BrowserWidget :: resizeEvent(QResizeEvent * event)
{
   QWidget::resizeEvent(event);
   layOutOverlay();
}

void BrowserWidget :: showEvent(QShowEvent * event)
{
   QWidget::showEvent(event);
   layOutOverlay();   // the constructor ran before we had a real geometry
}

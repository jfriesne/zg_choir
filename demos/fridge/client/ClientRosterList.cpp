#include <QMenu>

#include "ClientRosterList.h"
#include "FridgeChatView.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)

namespace fridge {

ClientRosterList :: ClientRosterList(ITreeGateway * connector, const FridgeChatView * fcv)
   : ITreeGatewaySubscriber(connector)
   , _updateDisplayPending(false)
   , _fcv(fcv)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ShowContextMenu(const QPoint &)));

   (void) AddTreeSubscription("clients/*/*/*/clientinfo");  // *'s are for:  peerID, clientIPAddress, sessionID
   (void) AddTreeSubscription("clients/*/*/*/clientinfo/crl_target");  // just so we can get MessageReceivedFromSubscriber() callbacks when someone targets our crl_target node
}

ClientRosterList :: ~ClientRosterList()
{
   // empty
}

void ClientRosterList :: TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg)
{
   ITreeGatewaySubscriber::TreeNodeUpdated(nodePath, optPayloadMsg);

   if ((nodePath.StartsWith("clients/"))&&(nodePath.EndsWith("/clientinfo")))
   {
      if (optPayloadMsg())
      {
printf("ClientRosterNodePath=[%s]\n", nodePath());
         (void) _clientRoster.Put(nodePath, optPayloadMsg);
         SetUpdateDisplayPending();
      }
      else if (_clientRoster.Remove(nodePath).IsOK()) SetUpdateDisplayPending();
   }
}

void ClientRosterList :: TreeGatewayConnectionStateChanged()
{
   ITreeGatewaySubscriber::TreeGatewayConnectionStateChanged();
   if (IsTreeGatewayConnected()) 
   {
      _clientRoster.Clear();
      FlushUpdateDisplay();

      // Upload a dummy node to "crl_target" solely so that we can subscribe to it and get notified when
      // anyone calls SendMessageToSubscriber() with a path that specifies our node
      (void) UploadTreeNodeValue("clients/clientinfo/crl_target", GetMessageFromPool());
   }
}

void ClientRosterList :: CallbackBatchEnds()
{
   ITreeGatewaySubscriber::CallbackBatchEnds();
   FlushUpdateDisplay();
}

void ClientRosterList :: UpdateDisplay()
{
   const QString prevSel = currentItem() ? currentItem()->text() : QString();

   clear();
   for (HashtableIterator<String, MessageRef> iter(_clientRoster); iter.HasData(); iter++)
   {
      const QString nextClientName = iter.GetValue()()->GetString("user")();

      QListWidgetItem * lwi = new QListWidgetItem(nextClientName);
      lwi->setData(Qt::UserRole, iter.GetKey()());  // so we can look up the client's key-path from the QListWidget easily
      addItem(lwi);

      if (nextClientName == prevSel) setCurrentRow(count()-1);
   }
}

void ClientRosterList :: ShowContextMenu(const QPoint & pos)
{
   QListWidgetItem * item = itemFromIndex(indexAt(pos));
   if (item)
   {
      _pingTargetPath = item->data(Qt::UserRole).toString().toUtf8().constData();
      QMenu pm(this);
      pm.addAction(tr("Ping %1").arg(item->text()), this, SLOT(PingUser()));
      (void) pm.exec(mapToGlobal(pos));
      _pingTargetPath.Clear();
   }
   else
   {
      QMenu pm(this);
      pm.addAction(tr("Ping All Clients"), this, SLOT(PingUser()));
      (void) pm.exec(mapToGlobal(pos));
   }
}

enum {
   CLIENT_ROSTER_COMMAND_PING = 1885957735,  // 'ping'
   CLIENT_ROSTER_COMMAND_PONG,
};

void ClientRosterList :: PingUser()
{
printf("PING [%s]\n", _pingTargetPath());

   MessageRef pingMsg = GetMessageFromPool(CLIENT_ROSTER_COMMAND_PING);
   if ((pingMsg())&&(pingMsg()->AddString("from", _fcv?_fcv->GetLocalUserName().toUtf8().constData():"Somebody").IsOK()))
   {
      if (_pingTargetPath.HasChars()) (void) SendMessageToSubscriber(_pingTargetPath.WithSuffix("/").WithSuffix("crl_target"), pingMsg);
                                 else (void) SendMessageToSubscriber("clients/*/*/*/clientinfo/crl_target", pingMsg);  // broadcast ping!
   }
}

}; // end namespace fridge

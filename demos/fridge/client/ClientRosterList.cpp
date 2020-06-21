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
   MessageRef pingMsg = GetMessageFromPool(CLIENT_ROSTER_COMMAND_PING);
   if ((pingMsg())&&(pingMsg()->AddString("user", _fcv?_fcv->GetLocalUserName().toUtf8().constData():"Somebody").IsOK()))
   {
      if (_pingTargetPath.HasChars()) (void) SendMessageToSubscriber(_pingTargetPath.WithSuffix("/").WithSuffix("crl_target"), pingMsg);
                                 else (void) SendMessageToSubscriber("clients/*/*/*/clientinfo/crl_target", pingMsg);  // broadcast ping!
   }
}

void ClientRosterList :: MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress)
{
   status_t ret;
   switch(payload()->what)
   {
      case CLIENT_ROSTER_COMMAND_PING:
         emit AddChatMessage(tr("Received Ping from [%1] at [%2]").arg(payload()->GetCstr("user")).arg(returnAddress()));
         payload()->what = CLIENT_ROSTER_COMMAND_PONG;   // turn it around and send it back as a pong
         (void) payload()->ReplaceString(true, "user", _fcv?_fcv->GetLocalUserName().toUtf8().constData():"Somebody");
         if (SendMessageToSubscriber(returnAddress, payload).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't send pong!  [%s]\n", ret());
      break;

      case CLIENT_ROSTER_COMMAND_PONG:
         emit AddChatMessage(tr("Received Pong from [%1] at [%2]").arg(payload()->GetCstr("user")).arg(returnAddress()));
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "cSendMessageToSubscriber::MessageReceivedFromSubscriber():  Unknown Message received!  nodePath=[%s] returnAddress=[%s]\n", nodePath(), returnAddress());
         payload()->PrintToStream();
      break;
   }
}

}; // end namespace fridge

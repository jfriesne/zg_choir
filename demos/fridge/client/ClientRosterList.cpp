#include <QMenu>

#include "ClientRosterList.h"
#include "FridgeChatView.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)
#include "zg/messagetree/gateway/TreeConstants.h" // for ZG_PARAMETER_NAME_*
#include "reflector/StorageReflectConstants.h"    // for PR_NAME_SESSION_ROOT

namespace fridge {

ClientRosterList :: ClientRosterList(ITreeGateway * connector, const FridgeChatView * fcv)
   : ITreeGatewaySubscriber(connector)
   , _updateDisplayPending(false)
   , _fcv(fcv)
{
   setSelectionMode(NoSelection);

   _clearColorsTimer.setSingleShot(true);
   connect(&_clearColorsTimer, SIGNAL(timeout()), this, SLOT(ClearColors()));

   setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ShowContextMenu(const QPoint &)));

   (void) AddTreeSubscription("clients/*/*/*/clientinfo");  // subscribe to all clientinfo nodes (*'s are for:  any ZGPeerID, any clientIPAddress, any sessionID)
}

ClientRosterList :: ~ClientRosterList()
{
   // empty
}

void ClientRosterList :: TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & optOpTag)
{
   ITreeGatewaySubscriber::TreeNodeUpdated(nodePath, optPayloadMsg, optOpTag);

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

   if (_localClientInfoNodePath.HasChars())
   {
      (void) RemoveTreeSubscription(_localClientInfoNodePath+"/crl_target");  // remove old crl_target subscription, it's obsolete because our sessionID has changed now
      _localClientInfoNodePath.Clear();
   }

   if (IsTreeGatewayConnected())
   {
      ConstMessageRef gestaltMsg = GetGestaltMessage();
      if (gestaltMsg())
      {
         // Subscribe to our client's own crl_target node just so that when someone calls SendMessageToSubscriber() on it, we'll get the Message
         _localClientInfoNodePath = String("clients/%1%2/clientinfo").Arg(gestaltMsg()->GetString(ZG_PARAMETER_NAME_PEERID)).Arg(gestaltMsg()->GetString(PR_NAME_SESSION_ROOT));
         (void) AddTreeSubscription(_localClientInfoNodePath+"/crl_target");
      }

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
   for (HashtableIterator<String, ConstMessageRef> iter(_clientRoster); iter.HasData(); iter++)
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
   if ((pingMsg())
    && (pingMsg()->AddString("user", _fcv?_fcv->GetLocalUserName().toUtf8().constData():"Somebody").IsOK())
    && (pingMsg()->AddString("key",  _localClientInfoNodePath).IsOK()))
   {
      if (_pingTargetPath.HasChars()) (void) SendMessageToSubscriber(_pingTargetPath.WithSuffix("/").WithSuffix("crl_target"), pingMsg);  // unicast ping
                                 else (void) SendMessageToSubscriber("clients/*/*/*/clientinfo/crl_target", pingMsg);                     // broadcast ping
   }
}

void ClientRosterList :: MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress)
{
   const QString fromUserName = payload()->GetString("user", "???")();
   const QString fromUserKey  = payload()->GetString("key")();

   status_t ret;
   switch(payload()->what)
   {
      case CLIENT_ROSTER_COMMAND_PING:
      {
         FlagUser(fromUserKey, false);
         emit AddChatMessage(tr("Received Ping from [%1] at [%2]").arg(fromUserName).arg(returnAddress()));
         payload()->what = CLIENT_ROSTER_COMMAND_PONG;   // turn it around and send it back as a pong
         (void) payload()->ReplaceString(true, "user", _fcv?_fcv->GetLocalUserName().toUtf8().constData():"Somebody");
         (void) payload()->ReplaceString(true, "key",  _localClientInfoNodePath);
         if (SendMessageToSubscriber(returnAddress, payload).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't send pong!  [%s]\n", ret());
      }
      break;

      case CLIENT_ROSTER_COMMAND_PONG:
         FlagUser(fromUserKey, true);
         emit AddChatMessage(tr("Received Pong from [%1] at [%2]").arg(fromUserName).arg(returnAddress()));
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "ClientRosterList::MessageReceivedFromSubscriber():  Unknown Message received!  nodePath=[%s] returnAddress=[%s]\n", nodePath(), returnAddress());
         payload()->Print(stdout);
      break;
   }
}

// Sets the text-color of a given user's name to red or blue, and restarts the timer to clear it in 250mS
void ClientRosterList :: FlagUser(const QString & key, bool isPong)
{
   for (int i=0; i<count(); i++)
   {
      QListWidgetItem * lwi = item(i);
      if (lwi->data(Qt::UserRole).toString() == key)
      {
         lwi->setBackground(isPong ? Qt::blue : Qt::red);
         lwi->setForeground(Qt::white);
         _clearColorsTimer.stop();
         _clearColorsTimer.start(500);
         break;
      }
   }
}

void ClientRosterList :: ClearColors()
{
   for (int i=0; i<count(); i++)
   {
      QListWidgetItem * lwi = item(i);
      lwi->setBackground(palette().brush(QPalette::Base));
      lwi->setForeground(palette().brush(QPalette::Text));
   }
}

}; // end namespace fridge

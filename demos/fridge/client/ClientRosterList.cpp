#include "ClientRosterList.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)

namespace fridge {

ClientRosterList :: ClientRosterList(ITreeGateway * connector)
   : ITreeGatewaySubscriber(connector)
   , _updateDisplayPending(false)
{
   (void) AddTreeSubscription("clients/*/*/*/clientinfo");  // *'s are for:  peerID, clientIPAddress, sessionID
}

ClientRosterList :: ~ClientRosterList()
{
   // empty
}

void ClientRosterList :: TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg)
{
   ITreeGatewaySubscriber::TreeNodeUpdated(nodePath, optPayloadMsg);

   if (nodePath.StartsWith("clients/"))
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
      addItem(nextClientName);
      if (nextClientName == prevSel) setCurrentRow(count()-1);
   }
}

}; // end namespace fridge

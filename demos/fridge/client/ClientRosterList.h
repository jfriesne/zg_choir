#ifndef ClientRosterList_h
#define ClientRosterList_h

#include <QListWidget>

#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "common/FridgeNameSpace.h"

namespace fridge {

class FridgeChatView;

/** This view shows a list of the currently-online clients' names */
class ClientRosterList : public QListWidget, public ITreeGatewaySubscriber
{
Q_OBJECT

public:
   /** Constructor
     * @param connector the ITreeGateway we should register with and use for our database access
     * @param fcv Pointer to the FridgeChatView that created us, just so we can get our local user's name from it when necessary
     */
   ClientRosterList(ITreeGateway * connector, const FridgeChatView * fcv);

   /** Destructor */
   virtual ~ClientRosterList();

   // ITreeGatewaySubscriber API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void CallbackBatchEnds();

private slots:
   void SetUpdateDisplayPending() {_updateDisplayPending = true;}
   void FlushUpdateDisplay() {if (_updateDisplayPending) {_updateDisplayPending = false; UpdateDisplay();}}
   void ShowContextMenu(const QPoint &);
   void PingUser();

private:
   void UpdateDisplay();

   struct CompareMessageRefFunctor
   {
      int Compare(const MessageRef & m1, const MessageRef & m2, void *) const
      {
         return muscleCompare(m1()->GetString("user"), m2()->GetString("user"));
      }
   };
   OrderedValuesHashtable<String, MessageRef, CompareMessageRefFunctor> _clientRoster;
   bool _updateDisplayPending;

   String _pingTargetPath;  // used during PingUser() calls
   const FridgeChatView * _fcv;
};

}; // end namespace fridge

#endif

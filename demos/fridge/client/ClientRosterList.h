#ifndef ClientRosterList_h
#define ClientRosterList_h

#include <QListWidget>

#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "common/FridgeNameSpace.h"

namespace fridge {

/** This view shows a list of the currently-online clients' names */
class ClientRosterList : public QListWidget, public ITreeGatewaySubscriber
{
Q_OBJECT

public:
   /** Constructor
     * @param connector the ITreeGateway we should register with and use for our database access
     */
   ClientRosterList(ITreeGateway * connector);

   /** Destructor */
   virtual ~ClientRosterList();

   // ITreeGatewaySubscriber API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void CallbackBatchEnds();

private slots:
   void SetUpdateDisplayPending() {_updateDisplayPending = true;}
   void FlushUpdateDisplay() {if (_updateDisplayPending) {_updateDisplayPending = false; UpdateDisplay();}}

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
};

}; // end namespace fridge

#endif

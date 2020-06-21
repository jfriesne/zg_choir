#ifndef FridgeChatView_h
#define FridgeChatView_h

#include <QWidget>

#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "util/Hashtable.h"
#include "ChatTextEntry.h"

class QLineEdit;
class QListWidgetItem;
class QString;
class QTextEdit;

namespace fridge {

class ClientRosterList;

/** This view is a chat-text pane, including read-only chat-text view and line-edits for your username and chat text entry */
class FridgeChatView : public QWidget, public ITreeGatewaySubscriber
{
Q_OBJECT

public:
   /** Constructor
     * @param connector the ITreeGateway we should register with and use for our database access
     * @param initialUserName what our local user name should be set to, initially
     */
   FridgeChatView(ITreeGateway * connector, const QString & initialUserName);

   /** Destructor */
   virtual ~FridgeChatView();

   // ITreeGatewaySubscriber API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void CallbackBatchEnds();

   void AcceptKeyPressEventFromWindow(QKeyEvent * e);

   QString GetLocalUserName() const;

public slots:
   void ClearChat();

private slots:
   void SetUpdateDisplayPending() {_updateDisplayPending = true;}
   void FlushUpdateDisplay() {if (_updateDisplayPending) {_updateDisplayPending = false; UpdateDisplay();}}
   void UploadNewUserName();
   void UploadNewChatLine();
   void UserNameDoubleClicked(QListWidgetItem *);
   status_t UploadNewChatLine(const QString & chatText);
 
private:
   void UpdateDisplay();

   struct CompareChatTextEntriesFunctor
   {
      int Compare(const ChatTextEntry & cte1, const ChatTextEntry & cte2, void *) const
      {
         return cte1.CompareTo(cte2);
      }
   };
   OrderedValuesHashtable<int32, ChatTextEntry, CompareChatTextEntriesFunctor> _chatData;
   bool _updateDisplayPending;

   QTextEdit * _chatText;
   QLineEdit * _userName;
   QLineEdit * _chatLine;

   ClientRosterList * _clientRosterList;
};

}; // end namespace fridge

#endif

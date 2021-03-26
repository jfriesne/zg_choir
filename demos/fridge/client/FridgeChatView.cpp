#include <QBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QToolButton>
#include <QScrollBar>
#include <QTextEdit>

#include "ClientRosterList.h"
#include "FridgeChatView.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)

namespace fridge {

FridgeChatView :: FridgeChatView(ITreeGateway * connector, const QString & initialUserName)
   : ITreeGatewaySubscriber(connector)
   , _updateDisplayPending(false)
{
   QBoxLayout * hbl = new QBoxLayout(QBoxLayout::LeftToRight, this);
   hbl->setMargin(5);

   QWidget * leftWidget = new QWidget;
   {
      QBoxLayout * vbl = new QBoxLayout(QBoxLayout::TopToBottom, leftWidget);
      vbl->setMargin(0);

      _chatText = new QTextEdit;
      _chatText->setReadOnly(true);
      vbl->addWidget(_chatText, 1);

      QWidget * bottomWidget = new QWidget;
      {
         QBoxLayout * bottomLayout = new QBoxLayout(QBoxLayout::LeftToRight, bottomWidget);
         bottomLayout->setMargin(0);

         _userName = new QLineEdit;
         _userName->setFixedWidth(100);
         _userName->setText(initialUserName);
         connect(_userName, SIGNAL(editingFinished()), this, SLOT(UploadNewUserName()));
         bottomLayout->addWidget(_userName);
        
         _chatLine = new QLineEdit;
         connect(_chatLine, SIGNAL(returnPressed()), this, SLOT(UploadNewChatLine()));
         bottomLayout->addWidget(_chatLine, 1);

         QToolButton * clearChat = new QToolButton;
         clearChat->setText(tr("Clear Chat"));
         connect(clearChat, SIGNAL(clicked()), this, SLOT(ClearChat()));
         bottomLayout->addWidget(clearChat, 1);
      }
      vbl->addWidget(bottomWidget);
   }
   hbl->addWidget(leftWidget, 1);

   _clientRosterList = new ClientRosterList(connector, this);
   connect(_clientRosterList, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(UserNameDoubleClicked(QListWidgetItem *)));
   connect(_clientRosterList, SIGNAL(AddChatMessage(const QString &)), this, SLOT(UploadNewChatLine(const QString &)));
   _clientRosterList->setFixedWidth(100);
   hbl->addWidget(_clientRosterList);

   (void) AddTreeSubscription("chat/*");
}

FridgeChatView :: ~FridgeChatView()
{
   // empty
}

void FridgeChatView :: TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg, const String & optOpTag)
{
   ITreeGatewaySubscriber::TreeNodeUpdated(nodePath, optPayloadMsg, optOpTag);

   if (nodePath.StartsWith("chat/"))
   {
      const int32 chatEntryID = atoi(nodePath()+5);
      if (optPayloadMsg())
      {
         ChatTextEntry cte;
         if (cte.SetFromArchive(*optPayloadMsg()).IsOK()) (void) _chatData.Put(chatEntryID, cte);
         SetUpdateDisplayPending();
      }
      else if (_chatData.Remove(chatEntryID).IsOK()) SetUpdateDisplayPending();
   }
}

void FridgeChatView :: TreeGatewayConnectionStateChanged()
{
   ITreeGatewaySubscriber::TreeGatewayConnectionStateChanged();
   if (IsTreeGatewayConnected()) 
   {
      _chatData.Clear();
      FlushUpdateDisplay();
      UploadNewUserName();
   }
}

void FridgeChatView :: CallbackBatchEnds()
{
   ITreeGatewaySubscriber::CallbackBatchEnds();
   FlushUpdateDisplay();
}

void FridgeChatView :: UpdateDisplay()
{
   String t;
   for (HashtableIterator<int32, ChatTextEntry> iter(_chatData); iter.HasData(); iter++)
   {
      if (t.HasChars()) t += '\n';
      t += iter.GetValue().ToString();
   }
   _chatText->setPlainText(t());
   _chatText->verticalScrollBar()->setValue(_chatText->verticalScrollBar()->maximum());
}

void FridgeChatView :: ClearChat()
{
   (void) RequestDeleteTreeNodes("chat/*");
}

void FridgeChatView :: UploadNewUserName()
{
   MessageRef newMsg = GetMessageFromPool();
   if ((newMsg())&&(newMsg()->AddString("user", _userName->text().trimmed().toUtf8().constData()).IsOK())) (void) UploadTreeNodeValue("clients/clientinfo", newMsg);
}

void FridgeChatView :: UploadNewChatLine()
{
   if (UploadNewChatLine(_chatLine->text().trimmed().toUtf8().constData()).IsOK()) _chatLine->clear();
}

status_t FridgeChatView :: UploadNewChatLine(const QString & chatText)
{
   // TODO: generate a better timestamp -- GetCurrentTime64() will return different times on different client machines whose wallclocks are set differently, leading to weirdness
   const ChatTextEntry cte(chatText.trimmed().toUtf8().constData(), _userName->text().trimmed().toUtf8().constData(), GetCurrentTime64(MUSCLE_TIMEZONE_LOCAL));
   MessageRef cteMsg = GetMessageFromPool();
   MRETURN_OOM_ON_NULL(cteMsg());
   MRETURN_ON_ERROR(cte.SaveToArchive(*cteMsg()));

   (void) UploadTreeNodeValue("chat/", cteMsg);

   // If our table starts getting too large, we should start deleting old chat text to avoid an eventual slowdown
   if (_chatData.GetNumItems() >= 500) (void) UploadTreeNodeValue(String("chat/%1").Arg(*_chatData.GetFirstKey()), MessageRef());

   return B_NO_ERROR;
}

void FridgeChatView :: AcceptKeyPressEventFromWindow(QKeyEvent * e)
{
   if (_userName->hasFocus() == false)
   {
      const QString t = e->text();
      if (t.size() > 0)
      {
         _chatLine->setFocus();
         _chatLine->setText(_chatLine->text()+t);
         _chatLine->setCursorPosition(_chatLine->text().size());
      }
   }
}

void FridgeChatView :: UserNameDoubleClicked(QListWidgetItem * item)
{
   (void) UploadNewChatLine(item->text());
}

QString FridgeChatView :: GetLocalUserName() const 
{
   return _userName->text();
}

}; // end namespace fridge

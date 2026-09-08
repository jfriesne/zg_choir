#include "MessagePanel.h"

#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>

#include "Theme.h"

#include "zlib/ZLibUtilityFunctions.h"   // for IsMessageDeflated()/InflateMessage()

using namespace muscle;

MessagePanel :: MessagePanel(QWidget * parent)
   : QWidget(parent)
{
   setAutoFillBackground(true);
   setStyleSheet(QString("MessagePanel { background: %1; }").arg(zgb::theme::contentBackground.name()));

   QVBoxLayout * layout = new QVBoxLayout(this);
   layout->setContentsMargins(8, 6, 8, 6);
   layout->setSpacing(4);

   _pathLabel = new QLabel;
   {
      QFont f = _pathLabel->font();
      f.setBold(true);
      f.setPointSizeF(f.pointSizeF() + 1.0);
      _pathLabel->setFont(f);
   }
   layout->addWidget(_pathLabel);

   _contents = new QPlainTextEdit;
   _contents->setReadOnly(true);
   _contents->setLineWrapMode(QPlainTextEdit::NoWrap);   // the dump is pre-formatted; scroll instead
   _contents->setFrameShape(QFrame::NoFrame);
   _contents->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
   layout->addWidget(_contents, 1);

   clear();
}

void MessagePanel :: clear()
{
   _pathLabel->setText(tr("No node selected"));
   _contents->setPlainText(tr("Select a node in the tree to see its Message."));
}

void MessagePanel :: showNode(const String & nodePath, const ConstMessageRef & optPayload)
{
   _pathLabel->setText(zgb::toQt(nodePath.WithPrepend("/")));

   QString text;
   if (optPayload())
   {
      text += zgb::toQt(optPayload()->ToString());

      if (IsMessageDeflated(optPayload))
      {
         const ConstMessageRef inflated = InflateMessage(optPayload);
         text += "\n\n--- inflates to: ---\n\n";
         if (inflated()) text += zgb::toQt(inflated()->ToString());
                    else text += QString("[inflate error: %1]").arg(inflated.GetStatus()());
      }
   }
   else text = tr("This node's payload isn't known (it may have just been deleted).");

   _contents->setPlainText(text);
   _contents->moveCursor(QTextCursor::Start);
   _contents->verticalScrollBar()->setValue(0);
   _contents->horizontalScrollBar()->setValue(0);
}

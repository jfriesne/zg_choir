#ifndef MessagePanel_h
#define MessagePanel_h

#include <QWidget>

#include "message/Message.h"

#include "ZGBrowserNameSpace.h"

class QLabel;
class QPlainTextEdit;

namespace zg_browser {

/** Shows the Message payload of the node currently selected in the tree. */
class MessagePanel MUSCLE_FINAL_CLASS : public QWidget
{
   Q_OBJECT

public:
   explicit MessagePanel(QWidget * parent = NULL);

   /** Shows (optPayload) as the payload of the node at (nodePath).
     * A NULL (optPayload) means we don't have this node's payload.
     */
   void showNode(const String & nodePath, const ConstMessageRef & optPayload);

   /** Reverts to the "nothing selected" state. */
   void clear();

private:
   QLabel * _pathLabel;
   QPlainTextEdit * _contents;
};

}  // end zg_browser namespace

#endif  // MessagePanel_h

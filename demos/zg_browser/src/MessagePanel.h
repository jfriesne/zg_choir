#pragma once

#include <QWidget>

#include "MuscleQt.h"

#include "message/Message.h"

class QLabel;
class QPlainTextEdit;

/** Shows the Message payload of the node currently selected in the tree. */
class MessagePanel final : public QWidget
{
   Q_OBJECT

public:
   explicit MessagePanel(QWidget * parent = NULL);

   /** Shows (optPayload) as the payload of the node at (nodePath).
     * A NULL (optPayload) means we don't have this node's payload.
     */
   void showNode(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayload);

   /** Reverts to the "nothing selected" state. */
   void clear();

private:
   QLabel * _pathLabel;
   QPlainTextEdit * _contents;
};

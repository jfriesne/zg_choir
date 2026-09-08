#include "NodeTreeItem.h"

#include <QCollator>
#include <QTreeWidget>

#include "Theme.h"

using namespace muscle;

namespace {

/** Natural ("srv2" before "srv10") comparison, so node names sort the way a user expects. */
int compareNatural(const QString & a, const QString & b)
{
   static const QCollator collator = []
   {
      QCollator c;
      c.setNumericMode(true);
      c.setCaseSensitivity(Qt::CaseInsensitive);
      return c;
   }();
   return collator.compare(a, b);
}

}  // anonymous namespace

NodeTreeItem :: NodeTreeItem(QTreeWidget * parent)
   : QTreeWidgetItem(parent, QStringList("/"))
{
   initCommon();
}

NodeTreeItem :: NodeTreeItem(const String & name)
   : QTreeWidgetItem(QStringList(zgb::toQt(name)))
   , _name(name)
{
   initCommon();
}

void NodeTreeItem :: initCommon()
{
   // Every node might have children; the only way to find out is to subscribe.
   setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

   setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
   setForeground(1, zgb::theme::textDim);
}

String NodeTreeItem :: getNodePath() const
{
   const NodeTreeItem * p = dynamic_cast<const NodeTreeItem *>(parent());
   if (p == NULL) return GetEmptyString();   // we're the root node

   const String parentPath = p->getNodePath();
   return parentPath.IsEmpty() ? _name : (parentPath + "/" + _name);
}

NodeTreeItem * NodeTreeItem :: getChildByName(const String & name) const
{
   for (int i=0; i<childCount(); i++)
   {
      NodeTreeItem * ch = dynamic_cast<NodeTreeItem *>(child(i));
      if (ch->getNodeName() == name) return ch;
   }
   return NULL;
}

NodeTreeItem * NodeTreeItem :: addChildNode(const String & name)
{
   const QString newName = zgb::toQt(name);

   int insertAt = childCount();
   for (int i=0; i<childCount(); i++)
   {
      const NodeTreeItem * ch = dynamic_cast<const NodeTreeItem *>(child(i));
      if (compareNatural(newName, zgb::toQt(ch->getNodeName())) < 0) {insertAt = i; break;}
   }

   NodeTreeItem * newItem = new NodeTreeItem(name);   // parentless, so we can insert it where we want
   insertChild(insertAt, newItem);
   return newItem;
}

void NodeTreeItem :: removeChildNode(const String & name)
{
   for (int i=0; i<childCount(); i++)
   {
      const NodeTreeItem * ch = dynamic_cast<const NodeTreeItem *>(child(i));
      if (ch->getNodeName() == name) {delete takeChild(i); return;}
   }
}

void NodeTreeItem :: clearChildren()
{
   for (int i=childCount()-1; i>=0; i--) delete takeChild(i);
}

void NodeTreeItem :: setSummary(const QString & summary)
{
   setText(1, summary);
}

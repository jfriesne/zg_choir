#include "NodeTreeItem.h"

namespace zg_browser {

NodeTreeItem :: NodeTreeItem(QTreeWidget * parent)
   : QTreeWidgetItem(parent, QStringList("/"))
{
   initCommon();
}

NodeTreeItem :: NodeTreeItem(const String & name)
   : QTreeWidgetItem(QStringList(ToQ(name)))
   , _name(name)
{
   initCommon();
}

void NodeTreeItem :: initCommon()
{
   // Every node might have children; the only way to find out is to subscribe.
   setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

   setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

   QColor c = foreground(0).color();
   c.setAlpha(200);
   setForeground(1, c);
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
   int insertAt = childCount();
   for (int i=0; i<childCount(); i++)
   {
      const NodeTreeItem * ch = dynamic_cast<const NodeTreeItem *>(child(i));
      if (name.NumericAwareCompareToIgnoreCase(ch->getNodeName()) < 0) {insertAt = i; break;}
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

}  // end namespace zg_browser

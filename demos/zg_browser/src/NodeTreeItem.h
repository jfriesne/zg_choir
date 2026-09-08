#ifndef NodeTreeItem_h
#define NodeTreeItem_h

#include <QTreeWidgetItem>

#include "ZGBrowserNameSpace.h"

namespace zg_browser {

/** One node of the server's database tree.
  *
  * Every item claims that it might contain sub-items (ShowIndicator), because
  * the only way to find out whether a node has children is to subscribe to
  * them -- which is exactly what opening the item does.
  */
class NodeTreeItem MUSCLE_FINAL_CLASS : public QTreeWidgetItem
{
public:
   /** Creates the root item ("/"), owned by the tree widget. */
   explicit NodeTreeItem(QTreeWidget * parent);

   /** Creates a parentless item named (name); addChildNode() puts it in its place. */
   explicit NodeTreeItem(const String & name);

   /** This node's name within its parent ("" for the root node). */
   MUSCLE_NODISCARD const String & getNodeName() const {return _name;}

   /** This node's session-relative path ("" for the root, "srv/foo" for a grandchild). */
   String getNodePath() const;

   MUSCLE_NODISCARD NodeTreeItem * getChildByName(const String & name) const;

   /** Creates a child item, inserted so that children stay in natural name order. */
   NodeTreeItem * addChildNode(const String & name);

   /** Deletes the named child item (and its descendants), if present. */
   void removeChildNode(const String & name);

   /** Deletes every child item (and their descendants). */
   void clearChildren();

   /** Sets the dim right-hand annotation shown in column 1 (eg "3 fields"). */
   void setSummary(const QString & summary);

   /** True iff we currently hold a subscription to this node's children. */
   MUSCLE_NODISCARD bool isSubscribed() const {return _subscribed;}
   void setSubscribed(bool subscribed) {_subscribed = subscribed;}

private:
   void initCommon();

   const String _name;
   bool _subscribed = false;
};

}  // end zg_browser namespace

#endif  // NodeTreeItem_h

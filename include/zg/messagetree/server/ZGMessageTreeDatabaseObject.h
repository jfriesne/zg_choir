#ifndef ZGMessageTreeDatabaseObject_h
#define ZGMessageTreeDatabaseObject_h

#include "zg/IDatabaseObject.h"

namespace zg
{

/** This is a concrete implementation of IDatabaseObject that uses a subtree of the MUSCLE 
  * Message-tree database as the data structure it synchronizes across peers.
  */
class ZGMessageTreeDatabaseObject : public IDatabaseObject
{
public:
   /** Constructor
     * @param session pointer to the ZGDatabasePeerSession object that created us
     * @param rootNodePath a sub-path indicating where the root of our managed Message sub-tree
     *                     should be located (relative to the ZGDatabaseSession's session-node)
     *                     May be empty if you want the session's session-node itself of be the
     *                     root of the managed sub-tree.
     */
   ZGMessageTreeDatabaseObject(ZGMessageTreeDatabasePeerSession * session, const String & rootNodePath);

   /** Destructor */
   virtual ~ZGMessageTreeDatabaseObject() {/* empty */}

   // IDatabaseObject implementation
   virtual void SetToDefaultState();
   virtual status_t SetFromArchive(const ConstMessageRef & archive);
   virtual status_t SaveToArchive(const MessageRef & archive) const;
   virtual uint32 GetCurrentChecksum() const {return _checksum;}
   virtual uint32 CalculateChecksum() const;
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);
   virtual String ToString() const;

   /** Returns a pointer to the ZGMessageTreeDatabasePeerSession object that created us, or NULL
     * if this object was not created by a ZGMessageTreeDatabasePeerSession.
     */
   ZGMessageTreeDatabasePeerSession * GetMessageTreeDatabasePeerSession() const {return static_cast<ZGMessageTreeDatabasePeerSession *>(GetDatabasePeerSession());}

private:
   void DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const;

   ZGMessageTreeDatabasePeerSession * _messageTreeSession;

   const String _rootNodePath;
   uint32 _checksum;  // running checksum
};
DECLARE_REFTYPES(ZGMessageTreeDatabaseObject);

};  // end namespace zg

#endif

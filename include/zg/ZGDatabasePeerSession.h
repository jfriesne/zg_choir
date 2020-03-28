#ifndef ZGDatabasePeerSession_h
#define ZGDatabasePeerSession_h

#include "zg/ZGPeerSession.h"
#include "zg/IDatabaseObject.h"

namespace zg
{

class ZGMessageTreeDatabaseObject;

/** This is a convenience class; it is the same as a ZGPeerSession except that it also knows
  * how to create and manage some user-provided IDatabaseObjects (one per database) so that 
  * the subclass doesn't have to implement all of the Message<->IDatabaseObject plumbing manually.
  */
class ZGDatabasePeerSession : public ZGPeerSession
{
public:
   /** Constructor
     * @param peerSettings the ZGPeerSettings that this system is to use.
     */
   ZGDatabasePeerSession(const ZGPeerSettings & peerSettings);

   /** Overridden to create and set up our IDatabaseObjects */
   virtual status_t AttachedToServer();

   /** Convenience method:  Returns a read/write pointer to the specified IDatabaseObject that we hold. 
     * @param whichDatabase The index of the database that we need an object to represent.
     */
   IDatabaseObject * GetDatabaseObject(uint32 whichDatabase) {return _databaseObjects[whichDatabase]();}

   /** Convenience method:  Returns a read-only pointer to the specified IDatabaseObject that we hold. 
     * @param whichDatabase The index of the database that we need an object to represent.
     */
   const IDatabaseObject * GetDatabaseObject(uint32 whichDatabase) const {return _databaseObjects[whichDatabase]();}

protected:
   /** This will be called as part of the startup sequence.  It should create
     * a new IDatabaseObject that will represent the specified database and return
     * a reference to it, for the ZGDatabasePeerSession to manage.
     * @param whichDatabase The index of the database that we need an object to represent.
     */
   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase) = 0;

   // ZGPeerSession API implementation
   virtual void ResetLocalDatabaseToDefault(uint32 whichDatabase, uint32 & dbChecksum);
   virtual ConstMessageRef SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg);
   virtual MessageRef SaveLocalDatabaseToMessage(uint32 whichDatabase) const;
   virtual status_t SetLocalDatabaseFromMessage(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & newDBStateMsg);
   virtual uint32 CalculateLocalDatabaseChecksum(uint32 whichDatabase) const;
   virtual String GetLocalDatabaseContentsAsString(uint32 whichDatabase) const;

private:
   friend class ZGMessageTreeDatabaseObject;

   Queue<IDatabaseObjectRef> _databaseObjects;
};
DECLARE_REFTYPES(ZGDatabasePeerSession);

};  // end namespace zg

#endif

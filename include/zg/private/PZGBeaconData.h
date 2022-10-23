#ifndef PZGBeaconData_h
#define PZGBeaconData_h

#include "zg/private/PZGDatabaseStateInfo.h"
#include "util/FlatCountable.h"
#include "util/Queue.h"
#include "util/String.h"

namespace zg_private
{

enum {
   PZG_BEACON_DATA = 2053595748 // 'zgbd' 
};

/** This class holds all the data that the senior peer will send out periodically to keep
  * the junior peers informed about the state of the senior databases.
  */
class PZGBeaconData : public FlatCountable
{
public:
   PZGBeaconData() {/* empty */}

   virtual bool IsFixedSize()     const   {return false;}
   virtual uint32 TypeCode()      const   {return PZG_BEACON_DATA;}
   virtual uint32 FlattenedSize() const   {return sizeof(uint32) + (_dbis.GetNumItems()*PZGDatabaseStateInfo::FlattenedSize());}

   virtual void Flatten(uint8 * buffer, uint32 flatSize) const;
   virtual status_t Unflatten(DataUnflattener & unflat);

   uint32 CalculateChecksum() const;

   bool operator == (const PZGBeaconData & rhs) const;
   bool operator != (const PZGBeaconData & rhs) const {return !(*this == rhs);}

   const Queue<PZGDatabaseStateInfo> & GetDatabaseStateInfos() const {return _dbis;}
   Queue<PZGDatabaseStateInfo> & GetDatabaseStateInfos() {return _dbis;}
   void SetDatabaseStateInfos(const Queue<PZGDatabaseStateInfo> & dbis) {_dbis = dbis;}

   void PrintToStream() const;
   String ToString() const;

private:
   Queue<PZGDatabaseStateInfo> _dbis;
};
DECLARE_REFTYPES(PZGBeaconData);

/** Returns a reference to an empty PZGBeaconData object from our object pool */
PZGBeaconDataRef GetBeaconDataFromPool();

};  // end namespace zg_private

#endif

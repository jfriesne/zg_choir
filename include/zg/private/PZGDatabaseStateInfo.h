#ifndef PZGDatabaseStateInfo_h
#define PZGDatabaseStateInfo_h

#include "zg/private/PZGNameSpace.h"
#include "support/Flattenable.h"

namespace zg_private
{

enum {
   PZG_DATABASE_STATE_INFO = 2053596258, // 'zgdb'
};

/** This class contains some advertised info about the current state of a given replicated-database.  This information is periodically
  * shared between peers so that they can keep tabs on each others' progress in updating their local databases.
  */
class PZGDatabaseStateInfo : public PseudoFlattenable<PZGDatabaseStateInfo>
{
public:
   PZGDatabaseStateInfo();
   PZGDatabaseStateInfo(const PZGDatabaseStateInfo & stateInfo);
   PZGDatabaseStateInfo(uint64 currentDatabaseID, uint64 oldestDatabaseIDInLog, uint32 dbChecksum);

   PZGDatabaseStateInfo & operator=(const PZGDatabaseStateInfo & rhs);

   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR bool IsFixedSize()     {return true;}
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR uint32 TypeCode()      {return PZG_DATABASE_STATE_INFO;}
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return sizeof(_currentDatabaseStateID)+sizeof(_oldestDatabaseIDInLog)+sizeof(_dbChecksum);}

   void Flatten(DataFlattener flat) const;
   status_t Unflatten(DataUnflattener & unflat);

   void Print(const OutputPrinter & p) const;
   MUSCLE_NODISCARD String ToString() const;

   MUSCLE_NODISCARD uint64 GetCurrentDatabaseStateID() const {return _currentDatabaseStateID;}
   MUSCLE_NODISCARD uint64 GetOldestDatabaseIDInLog()  const {return _oldestDatabaseIDInLog;}
   MUSCLE_NODISCARD uint32 GetDBChecksum()             const {return _dbChecksum;}

   /** Calculates and returns a 32-bit checksum based on all the current contents of this object; not to be confused with the DBChecksum field! */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const;

   bool operator == (const PZGDatabaseStateInfo & rhs) const;
   bool operator != (const PZGDatabaseStateInfo & rhs) const {return !(*this == rhs);}

private:
   uint64 _currentDatabaseStateID; // ID of the state this database is currently in, on the machine that created this PZGDatabaseStateInfo
   uint64 _oldestDatabaseIDInLog;  // ID of the oldest database update that is still in the update log, on the machine that created this PZGDatabaseStateInfo
   uint32 _dbChecksum;             // 32-bit checksum computed from the current state of the database
};

};  // end namespace zg_private

#endif

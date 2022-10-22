#include "util/DataFlattener.h"
#include "util/DataUnflattener.h"
#include "zg/private/PZGDatabaseStateInfo.h"
#include "zlib/ZLibUtilityFunctions.h"

namespace zg_private
{

PZGDatabaseStateInfo :: PZGDatabaseStateInfo()
   : _currentDatabaseStateID(0)
   , _oldestDatabaseIDInLog(0)
   , _dbChecksum(0)
{
   // empty
}

PZGDatabaseStateInfo :: PZGDatabaseStateInfo(const PZGDatabaseStateInfo & stateInfo)
   : _currentDatabaseStateID(stateInfo._currentDatabaseStateID)
   , _oldestDatabaseIDInLog(stateInfo._oldestDatabaseIDInLog)
   , _dbChecksum(stateInfo._dbChecksum)
{
   // empty
}

PZGDatabaseStateInfo :: PZGDatabaseStateInfo(uint64 dbID, uint64 oldestDatabaseIDInLog, uint32 dbChecksum)
   : _currentDatabaseStateID(dbID)
   , _oldestDatabaseIDInLog(oldestDatabaseIDInLog)
   , _dbChecksum(dbChecksum)
{
   // empty
}

PZGDatabaseStateInfo & PZGDatabaseStateInfo :: operator=(const PZGDatabaseStateInfo & rhs)
{
   _currentDatabaseStateID = rhs._currentDatabaseStateID;
   _oldestDatabaseIDInLog  = rhs._oldestDatabaseIDInLog;
   _dbChecksum             = rhs._dbChecksum;
   return *this;
}

void PZGDatabaseStateInfo :: Flatten(uint8 * buf, uint32 flatSize) const
{
   DataFlattener flat(buf, flatSize);
   flat.WriteInt64(_currentDatabaseStateID);
   flat.WriteInt64(_oldestDatabaseIDInLog);
   flat.WriteInt32(_dbChecksum);
}

status_t PZGDatabaseStateInfo :: Unflatten(const uint8 * buf, uint32 size)
{
   if (size < FlattenedSize()) return B_BAD_DATA;

   DataUnflattener unflat(buf, size);
   _currentDatabaseStateID = unflat.ReadInt64();
   _oldestDatabaseIDInLog  = unflat.ReadInt64();
   _dbChecksum             = unflat.ReadInt32();
   return unflat.GetStatus();
}
   
void PZGDatabaseStateInfo :: PrintToStream() const
{
   printf("%s\n", ToString()());
}

String PZGDatabaseStateInfo :: ToString() const
{
   return String("DBState:  curDBID=%1 oldestID=%2 dbChecksum=%3").Arg(_currentDatabaseStateID).Arg(_oldestDatabaseIDInLog).Arg(_dbChecksum);
}

uint32 PZGDatabaseStateInfo :: CalculateChecksum() const
{
   return CalculateChecksumForUint64(_currentDatabaseStateID) + (CalculateChecksumForUint64(_oldestDatabaseIDInLog)*3) + _dbChecksum;
}

bool PZGDatabaseStateInfo :: operator == (const PZGDatabaseStateInfo & rhs) const
{
   return ((_currentDatabaseStateID == rhs._currentDatabaseStateID)&&(_oldestDatabaseIDInLog == rhs._oldestDatabaseIDInLog)+(_dbChecksum == rhs._dbChecksum));
}

};  // end namespace zg_private

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

void PZGDatabaseStateInfo :: Flatten(uint8 *buffer) const
{
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT64(_currentDatabaseStateID)); buffer += sizeof(uint64);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT64(_oldestDatabaseIDInLog));  buffer += sizeof(uint64);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(_dbChecksum));             buffer += sizeof(uint32);   
}

status_t PZGDatabaseStateInfo :: Unflatten(const uint8 *buf, uint32 size)
{
   if (size < FlattenedSize()) return B_BAD_DATA;

   _currentDatabaseStateID = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(buf)); buf += sizeof(uint64);
   _oldestDatabaseIDInLog  = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(buf)); buf += sizeof(uint64);
   _dbChecksum             = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf)); buf += sizeof(uint32);
   return B_NO_ERROR;
}
   
void PZGDatabaseStateInfo :: PrintToStream() const
{
   printf("%s\n", ToString()());
}

String PZGDatabaseStateInfo :: ToString() const
{
   char buf[256];
   sprintf(buf, "DBState:  curDBID=" UINT64_FORMAT_SPEC " oldestID=" UINT64_FORMAT_SPEC " dbChecksum=" UINT32_FORMAT_SPEC , _currentDatabaseStateID, _oldestDatabaseIDInLog, _dbChecksum);
   return buf;
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

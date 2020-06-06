#include "zg/private/PZGBeaconData.h"

namespace zg_private
{

void PZGBeaconData :: Flatten(uint8 *buffer) const
{
   const uint32 numItems    = _dbis.GetNumItems();
   const uint32 dbiFlatSize = PZGDatabaseStateInfo::FlattenedSize();

   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(numItems)); buffer += sizeof(uint32);   
   for (uint32 i=0; i<numItems; i++) {_dbis[i].Flatten(buffer); buffer += dbiFlatSize;}
}

status_t PZGBeaconData :: Unflatten(const uint8 *buf, uint32 size)
{
   if (size < sizeof(uint32)) return B_BAD_DATA;

   const uint32 itemFlatSize = PZGDatabaseStateInfo::FlattenedSize();
   const uint32 newNumItems  = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf)); buf += sizeof(uint32); size -= sizeof(uint32);
   if (size < (newNumItems*itemFlatSize)) return B_BAD_DATA;

   status_t ret;
   if (_dbis.EnsureSize(newNumItems, true).IsError(ret)) return ret;

   for (uint32 i=0; i<newNumItems; i++)
   {
      if (_dbis[i].Unflatten(buf, itemFlatSize).IsError(ret)) return ret;
      buf += itemFlatSize; size -= itemFlatSize;
   }
   return B_NO_ERROR;
}

PZGBeaconDataRef GetBeaconDataFromPool()
{
   static ObjectPool<PZGBeaconData> _infoListPool;
   return PZGBeaconDataRef(_infoListPool.ObtainObject());
}

uint32 PZGBeaconData :: CalculateChecksum() const
{
   const uint32 numItems = _dbis.GetNumItems();

   uint32 ret = numItems;
   for (uint32 i=0; i<numItems; i++) ret += (i+1)*(_dbis[i].CalculateChecksum());
   return ret;
}

void PZGBeaconData :: PrintToStream() const
{
   puts(ToString()());
}

String PZGBeaconData :: ToString() const
{
   String ret;

   char buf[128];
   for (uint32 i=0; i<_dbis.GetNumItems(); i++)
   {
      muscleSprintf(buf, "   DBI #" UINT32_FORMAT_SPEC ": ", i);
      ret += buf;
      ret += _dbis[i].ToString();
      ret += '\n';
   }
   return ret;
}

bool PZGBeaconData :: operator == (const PZGBeaconData & rhs) const
{
   return (_dbis == rhs._dbis);
}

};  // end namespace zg_private

#include "zg/private/PZGBeaconData.h"

namespace zg_private
{

void PZGBeaconData :: Flatten(DataFlattener flat) const
{
   flat.WriteInt32(_dbis.GetNumItems());
   for (uint32 i=0; i<_dbis.GetNumItems(); i++) flat.WriteFlat(_dbis[i]);
}

status_t PZGBeaconData :: Unflatten(DataUnflattener & unflat)
{
   const uint32 newNumItems = unflat.ReadInt32();
   if (unflat.GetNumBytesAvailable() < (newNumItems*PZGDatabaseStateInfo::FlattenedSize())) return B_BAD_DATA;

   MRETURN_ON_ERROR(_dbis.EnsureSize(newNumItems, true));
   for (uint32 i=0; i<newNumItems; i++) MRETURN_ON_ERROR(unflat.ReadFlat(_dbis[i]));
   return unflat.GetStatus();
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

void PZGBeaconData :: Print(const OutputPrinter & p) const
{
   p.puts(ToString()());
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

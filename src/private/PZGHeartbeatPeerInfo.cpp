#include "zg/private/PZGHeartbeatPeerInfo.h"
#include "zlib/ZLibUtilityFunctions.h"

namespace zg_private
{

uint32 PZGHeartbeatPeerInfo :: CalculateChecksum() const
{
   uint32 ret = _peerID.CalculateChecksum();
   for (uint32 i=0; i<_timings.GetNumItems(); i++) ret += _timings[i].CalculateChecksum();  // their order doesn't matter
   return ret;
}

uint32 PZGHeartbeatPeerInfo :: FlattenedSize() const
{
   return ZGPeerID::FlattenedSize() + sizeof(uint32) + (_timings.GetNumItems()*PZGTimingInfo::FlattenedSize());  // uint32 so we can store a list-size count
}

void PZGHeartbeatPeerInfo :: Flatten(uint8 *buffer) const
{
   _peerID.Flatten(buffer);                                                buffer += ZGPeerID::FlattenedSize();
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(_timings.GetNumItems())); buffer += sizeof(uint32);
   for (uint32 i=0; i<_timings.GetNumItems(); i++)
   {
      _timings[i].Flatten(buffer);
      buffer += PZGTimingInfo::FlattenedSize();
   }
}

status_t PZGHeartbeatPeerInfo :: Unflatten(const uint8 *buf, uint32 size)
{
   if (size < (_peerID.FlattenedSize()+sizeof(uint32))) return B_BAD_DATA;

   const uint32 peerIDFlatSize = ZGPeerID::FlattenedSize();
   status_t ret;
   if (_peerID.Unflatten(buf, peerIDFlatSize).IsError(ret)) return ret;
   buf += peerIDFlatSize; size -= peerIDFlatSize;

   _timings.Clear();

   const uint32 numTimings = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf)); 
   buf += sizeof(uint32); size -= sizeof(uint32);
   const uint32 tiFlatSize = PZGTimingInfo::FlattenedSize();

   if (size < (numTimings*tiFlatSize))               return B_BAD_DATA;
   if (_timings.EnsureSize(numTimings).IsError(ret)) return ret;

   for (uint32 i=0; i<numTimings; i++)
   {
      if (_timings.AddTailAndGet()->Unflatten(buf, tiFlatSize).IsError(ret)) return ret;
      buf += tiFlatSize; size -= tiFlatSize;
   }
   return B_NO_ERROR;
}

void PZGHeartbeatPeerInfo :: PZGTimingInfo :: Flatten(uint8 * buf) const
{
   muscleCopyOut(buf, B_HOST_TO_LENDIAN_INT16(_sourceTag));         buf += sizeof(uint16);
   muscleCopyOut(buf, B_HOST_TO_LENDIAN_INT16(0));                  buf += sizeof(uint16);  // reserved/padding for now
   muscleCopyOut(buf, B_HOST_TO_LENDIAN_INT32(_heartbeatPacketID)); buf += sizeof(uint32);
   muscleCopyOut(buf, B_HOST_TO_LENDIAN_INT32(_dwellTimeMicros));   buf += sizeof(uint32);
}

status_t PZGHeartbeatPeerInfo :: PZGTimingInfo :: Unflatten(const uint8 * buf, uint32 size)
{
   if (size < FlattenedSize()) return B_BAD_DATA;

   _sourceTag         = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(buf)); buf += sizeof(uint16); size -= sizeof(uint16);
   /* just skip past the two reserved/padding bytes, for now */             buf += sizeof(uint16); size -= sizeof(uint16);
   _heartbeatPacketID = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf)); buf += sizeof(uint32); size -= sizeof(uint32);
   _dwellTimeMicros   = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf)); buf += sizeof(uint32); size -= sizeof(uint32);
   return B_NO_ERROR;
}

uint32 PZGHeartbeatPeerInfo :: PZGTimingInfo :: CalculateChecksum() const
{
   return ((uint32)_sourceTag) + _heartbeatPacketID + _dwellTimeMicros;
}

String PZGHeartbeatPeerInfo :: ToString() const
{
   String ret = _peerID.ToString();
   for (uint32 i=0; i<_timings.GetNumItems(); i++) ret += _timings[i].ToString().Prepend(" ");
   return ret;
}

String PZGHeartbeatPeerInfo :: PZGTimingInfo :: ToString() const
{
   char buf[128];
   muscleSprintf(buf, "[src=%u/packet=" UINT32_FORMAT_SPEC "/dwell=" UINT32_FORMAT_SPEC "]", _sourceTag, _heartbeatPacketID, _dwellTimeMicros);
   return buf;
}

void PZGHeartbeatPeerInfo :: PrintToStream() const
{
   puts(ToString()());
}

PZGHeartbeatPeerInfoRef GetPZGHeartbeatPeerInfoFromPool()
{
   static ObjectPool<PZGHeartbeatPeerInfo> _infoListPool;
   return PZGHeartbeatPeerInfoRef(_infoListPool.ObtainObject());
}

status_t PZGHeartbeatPeerInfo :: PutTimingInfo(uint16 srcTag, uint32 sourceHeartbeatPacketID, uint32 dwellTimeMicros)
{
   return _timings.AddTail(PZGTimingInfo(srcTag, sourceHeartbeatPacketID, dwellTimeMicros));
}

};  // end namespace zg_private

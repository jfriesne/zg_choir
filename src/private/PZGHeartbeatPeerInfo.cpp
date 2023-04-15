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

void PZGHeartbeatPeerInfo :: Flatten(DataFlattener flat) const
{
   flat.WriteFlat(_peerID);
   flat.WriteInt32(_timings.GetNumItems());
   for (uint32 i=0; i<_timings.GetNumItems(); i++) flat.WriteFlat(_timings[i]);
}

status_t PZGHeartbeatPeerInfo :: Unflatten(DataUnflattener & unflat)
{
   MRETURN_ON_ERROR(unflat.ReadFlat(_peerID));

   const uint32 numTimings = unflat.ReadInt32();
   if (unflat.GetNumBytesAvailable() < (numTimings*PZGTimingInfo::FlattenedSize())) return B_BAD_DATA;
   MRETURN_ON_ERROR(_timings.EnsureSize(numTimings, true));

   for (uint32 i=0; i<numTimings; i++) MRETURN_ON_ERROR(unflat.ReadFlat(_timings[i]));
   return unflat.GetStatus();
}

void PZGHeartbeatPeerInfo :: PZGTimingInfo :: Flatten(DataFlattener flat) const
{
   flat.WriteInt16(_sourceTag);
   flat.WriteInt16(0);                   // reserved/padding for now
   flat.WriteInt32(_heartbeatPacketID);
   flat.WriteInt32(_dwellTimeMicros);
}

status_t PZGHeartbeatPeerInfo :: PZGTimingInfo :: Unflatten(DataUnflattener & unflat)
{
   _sourceTag         = unflat.ReadInt16();
   (void)               unflat.ReadInt16();       /* just skip past the two reserved/padding bytes, for now */
   _heartbeatPacketID = unflat.ReadInt32();
   _dwellTimeMicros   = unflat.ReadInt32();
   return unflat.GetStatus();
}

uint32 PZGHeartbeatPeerInfo :: PZGTimingInfo :: CalculateChecksum() const
{
   return ((uint32)_sourceTag) + _heartbeatPacketID + _dwellTimeMicros;
}

String PZGHeartbeatPeerInfo :: ToString() const
{
   String ret = _peerID.ToString();
   for (uint32 i=0; i<_timings.GetNumItems(); i++) ret += _timings[i].ToString().WithPrepend(' ');
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

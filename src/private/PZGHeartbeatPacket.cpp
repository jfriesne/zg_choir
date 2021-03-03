#include "zg/private/PZGHeartbeatPacket.h"
#include "zg/private/PZGConstants.h"  // for PeerInfoToString()
#include "zlib/ZLibUtilityFunctions.h"

namespace zg_private
{

PZGHeartbeatPacket :: PZGHeartbeatPacket()
   : _heartbeatPacketID(0)
   , _systemKey(0)
   , _networkSendTimeMicros(0)
   , _tcpAcceptPort(0)
   , _peerType(0)
   , _peerUptimeSeconds(0)
   , _isFullyAttached(false)
{
   // empty
}

PZGHeartbeatPacket :: PZGHeartbeatPacket(const PZGHeartbeatSettings & hbSettings, uint32 uptimeSeconds, bool isFullyAttached, uint32 packetID)
{
   Initialize(hbSettings, uptimeSeconds, isFullyAttached, packetID);
}

void PZGHeartbeatPacket :: Initialize(const PZGHeartbeatSettings & hbSettings, uint32 uptimeSeconds, bool isFullyAttached, uint32 packetID)
{
   _heartbeatPacketID     = packetID;
   _systemKey             = hbSettings.GetSystemKey();
   _networkSendTimeMicros = 0;
   _tcpAcceptPort         = hbSettings.GetDataTCPPort();
   _peerType              = hbSettings.GetPeerType();
   _peerUptimeSeconds     = uptimeSeconds;
   _sourcePeerID          = hbSettings.GetLocalPeerID();
   _isFullyAttached       = isFullyAttached;
   _peerAttributesBuf     = hbSettings.GetPeerAttributesByteBuffer();
}

uint32 PZGHeartbeatPacket :: CalculateChecksum() const
{
   // _networkSendTimeMicros is deliberately not part of our checksum as it will be sent separately for better accuracy
   uint32 ret = _heartbeatPacketID + CalculateChecksumForUint64(_systemKey) + _tcpAcceptPort + _peerUptimeSeconds + (_isFullyAttached?666:0) + _sourcePeerID.CalculateChecksum() + _peerType;
   for (uint32 i=0; i<_orderedPeersList.GetNumItems(); i++) ret += (i+1)*(_orderedPeersList[i]()->CalculateChecksum());
   if (_peerAttributesBuf()) ret += _peerAttributesBuf()->CalculateChecksum();
   /* deliberately not including _peerAttributesMsg in the checksum since it is redundant with _peerAttributesBuf */
   return ret;
}

uint32 PZGHeartbeatPacket :: FlattenedSizeNotIncludingVariableLengthData() const
{
   return sizeof(uint32)                 // for PZG_HEARTBEAT_PACKET_TYPE_CODE
        + sizeof(_heartbeatPacketID)
        + sizeof(_systemKey)
        // _networkSendTimeMicros is deliberately not part of our flattened-size as it will be sent separately for better accuracy
        + sizeof(_tcpAcceptPort)
        + sizeof(_peerUptimeSeconds)
        + ZGPeerID::FlattenedSize()      // for _sourceePeerID
        + sizeof(_peerType)
        + sizeof(uint16)                 // for _peerType and _isFullyAttached
        + sizeof(uint16)                 // for _orderedPeersList.GetNumItems()
        + sizeof(uint16);                // reserved for now 
}

uint32 PZGHeartbeatPacket :: FlattenedSize() const
{
   uint32 ret = FlattenedSizeNotIncludingVariableLengthData();
   for (uint32 i=0; i<_orderedPeersList.GetNumItems(); i++) ret += _orderedPeersList[i]()->FlattenedSize();
   if (_peerAttributesBuf()) ret += _peerAttributesBuf()->FlattenedSize();

   /** Deliberately not including _peerAttributesMsg in the size as we send _peerAttributesBuf instead */
   return ret;
}

void PZGHeartbeatPacket :: Flatten(uint8 *buffer) const
{
   const uint32 peerIDFlatSize   = ZGPeerID::FlattenedSize();
   const uint32 opListItemCount  = _orderedPeersList.GetNumItems();
   const uint32 attribBufSize    = _peerAttributesBuf() ? _peerAttributesBuf()->GetNumBytes() : 0;

   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(PZG_HEARTBEAT_PACKET_TYPE_CODE));        buffer += sizeof(uint32);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(_heartbeatPacketID));                    buffer += sizeof(uint32);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT64(_systemKey));                            buffer += sizeof(uint64);
   // _networkSendTimeMicros is deliberately not part of our flattened-data as it will be sent separately for better accuracy
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT16(_tcpAcceptPort));                        buffer += sizeof(uint16);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(_peerUptimeSeconds));                    buffer += sizeof(uint32);
   _sourcePeerID.Flatten(buffer);                                                         buffer += peerIDFlatSize;
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT16(_peerType|(_isFullyAttached?0x8000:0))); buffer += sizeof(uint16);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT16((uint16)opListItemCount));               buffer += sizeof(uint16);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT16((uint16)attribBufSize));                 buffer += sizeof(uint16);
   muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT16((uint16)0)); /* reserved for now */      buffer += sizeof(uint16);

   for (uint32 i=0; i<opListItemCount; i++)
   {
      const PZGHeartbeatPeerInfo & pi = *_orderedPeersList[i]();
      pi.Flatten(buffer);
      buffer += pi.FlattenedSize(); 
   }

   if (attribBufSize > 0) {_peerAttributesBuf()->Flatten(buffer);                         buffer += attribBufSize;}
   /** Deliberately not flattening _peerAttributesMsg as it is redundant with _peerAttributesBuf */
}

status_t PZGHeartbeatPacket :: Unflatten(const uint8 *buf, uint32 size)
{
   const uint32 staticBytesNeeded = FlattenedSizeNotIncludingVariableLengthData();
   if (size < staticBytesNeeded)
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::Unflatten():  Got short buffer for step A (" UINT32_FORMAT_SPEC " < " UINT32_FORMAT_SPEC ")\n", size, staticBytesNeeded);
      return B_BAD_DATA;
   }

   const uint32 typeCode = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf)); buf += sizeof(uint32); size -= sizeof(uint32);
   if (typeCode != PZG_HEARTBEAT_PACKET_TYPE_CODE)
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::Unflatten():  Got unexpected heartbeat typecode " UINT32_FORMAT_SPEC "\n", typeCode);
      return B_BAD_DATA;
   }

   const uint32 peerIDFlatSize   = ZGPeerID::FlattenedSize();
   _heartbeatPacketID      = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf));  buf += sizeof(uint32); size -= sizeof(uint32);
   _systemKey              = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(buf));  buf += sizeof(uint64); size -= sizeof(uint64);
   _networkSendTimeMicros  = 0; // _networkSendTimeMicros is deliberately not part of our unflattened-data as it will be sent separately for better accuracy
   _tcpAcceptPort          = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(buf));  buf += sizeof(uint16); size -= sizeof(uint16);
   _peerUptimeSeconds      = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buf));  buf += sizeof(uint32); size -= sizeof(uint32);
   (void) _sourcePeerID.Unflatten(buf, peerIDFlatSize);                           buf += peerIDFlatSize; size -= peerIDFlatSize;
   _peerType               = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(buf));  buf += sizeof(uint16); size -= sizeof(uint16);
   _isFullyAttached        = ((_peerType & 0x8000) != 0); _peerType &= ~(0x8000);
   uint32 opListItemCount  = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(buf));  buf += sizeof(uint16); size -= sizeof(uint16);
   uint32 attribBufSize    = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(buf));  buf += sizeof(uint16); size -= sizeof(uint16);
   /* skip the currently-unused reserved field, for now */                        buf += sizeof(uint16); size -= sizeof(uint16);

   _orderedPeersList.Clear();

   status_t ret;
   if (_orderedPeersList.EnsureSize(opListItemCount).IsError(ret)) return ret;

   for (uint32 i=0; i<opListItemCount; i++)
   {
       PZGHeartbeatPeerInfoRef newPIRef = GetPZGHeartbeatPeerInfoFromPool();
       if (newPIRef() == NULL) MRETURN_OUT_OF_MEMORY;
       if (newPIRef()->Unflatten(buf, size).IsError(ret)) return ret;

       const uint32 actualPISize = newPIRef()->FlattenedSize();
       if (actualPISize > size) return B_BAD_DATA;  // wtf?
       buf  += actualPISize;
       size -= actualPISize;

       (void) _orderedPeersList.AddTail(newPIRef);
   }
   
   if (attribBufSize > 0) 
   {
      if (attribBufSize > size)
      {
         LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::Unflatten():  attribBufSize too large!  (" UINT32_FORMAT_SPEC " > " UINT32_FORMAT_SPEC ")\n", attribBufSize, size);
         return B_BAD_DATA;
      }
      _peerAttributesBuf = GetByteBufferFromPool(attribBufSize, buf);
      if (_peerAttributesBuf() == NULL) MRETURN_OUT_OF_MEMORY;
   }
   else _peerAttributesBuf.Reset();

   _peerAttributesMsg.Reset();  // this can be demand-allocated later, if necessary

   return B_NO_ERROR;
}

status_t PZGHeartbeatPacket :: CopyFromImplementation(const Flattenable & copyFrom)
{
   const PZGHeartbeatPacket * p = dynamic_cast<const PZGHeartbeatPacket *>(&copyFrom);
   if (p) {*this = *p; return B_NO_ERROR;}
     else return FlatCountable::CopyFromImplementation(copyFrom);
}

String PZGHeartbeatPacket :: ToString() const
{
   char buf[1024];
   muscleSprintf(buf, "Heartbeat:  PacketID=" UINT32_FORMAT_SPEC " sysKey=" UINT64_FORMAT_SPEC " netSendTime=" UINT64_FORMAT_SPEC " tcpPort=%u peerType=%u isFullyAttached=%i uptimeSeconds=" UINT32_FORMAT_SPEC " sourcePeerID=[%s]", _heartbeatPacketID, _systemKey, _networkSendTimeMicros, _tcpAcceptPort, _peerType, _isFullyAttached, _peerUptimeSeconds, _sourcePeerID.ToString()());

   String ret = buf;
   for (uint32 i=0; i<_orderedPeersList.GetNumItems(); i++) 
   {
      muscleSprintf(buf, "\n   OP #" UINT32_FORMAT_SPEC ": ", i);
      ret += buf;
      ret += _orderedPeersList[i]()->ToString();
   }

   ConstMessageRef attribMsg = GetPeerAttributesAsMessage();
   if (attribMsg()) 
   {
      ret += " ";
      ret += PeerInfoToString(attribMsg);
   }
   return ret;
}

void PZGHeartbeatPacket :: PrintToStream() const
{
   puts(ToString()());
   putchar('\n');
}

ConstMessageRef PZGHeartbeatPacket :: GetPeerAttributesAsMessage() const
{
   if ((_peerAttributesMsg() == NULL)&&(_peerAttributesBuf()))
   {
      // Demand-unflattend the Message object
      _peerAttributesMsg = GetMessageFromPool(InflateByteBuffer(_peerAttributesBuf));
      if (_peerAttributesMsg() == NULL) LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::GetPeerAttributes():  Couldn't unflatten byte buffer!\n");
   }
   return _peerAttributesMsg;
}

static bool AreByteBufferRefsEqual(const ConstByteBufferRef & optBB1, const ConstByteBufferRef & optBB2)
{
   if ((optBB1() != NULL) != (optBB2() != NULL)) return false;
   return ((optBB1() == NULL)||(*optBB1() == *optBB2()));
}

bool PZGHeartbeatPacket :: IsEqualIgnoreTransients(const PZGHeartbeatPacket & rhs) const
{
   return ((_systemKey        == rhs._systemKey)        &&
           (_tcpAcceptPort    == rhs._tcpAcceptPort)    &&
           (_peerType         == rhs._peerType)         &&
           (_sourcePeerID     == rhs._sourcePeerID)     &&
           (AreByteBufferRefsEqual(_peerAttributesBuf, rhs._peerAttributesBuf)));
}

PZGHeartbeatPacketWithMetaData :: PZGHeartbeatPacketWithMetaData()
   : PZGHeartbeatPacket()
   , _localReceiveTimeMicros(0)
   , _sourceTag(0)
   , _haveSentTimingReply(false)
{
   // empty
}
   
PZGHeartbeatPacketWithMetaData :: PZGHeartbeatPacketWithMetaData(const PZGHeartbeatSettings & hbSettings, uint32 uptimeSeconds, bool isFullyAttached, uint32 packetID)
   : PZGHeartbeatPacket(hbSettings, uptimeSeconds, isFullyAttached, packetID)
   , _localReceiveTimeMicros(0)
   , _sourceTag(0)
   , _haveSentTimingReply(false)
{
   // empty
}

};  // end namespace zg_private

#include "zg/private/PZGHeartbeatPacket.h"
#include "zg/private/PZGConstants.h"  // for PeerInfoToString()
#include "zlib/ZLibUtilityFunctions.h"

namespace zg_private
{

PZGHeartbeatPacket :: PZGHeartbeatPacket()
   : _heartbeatPacketID(0)
   , _versionCode(0)
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
   _versionCode           = hbSettings.GetVersionCode();
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
   uint32 ret = _heartbeatPacketID + _versionCode + CalculateChecksumForUint64(_systemKey) + _tcpAcceptPort + _peerUptimeSeconds + (_isFullyAttached?666:0) + _sourcePeerID.CalculateChecksum() + _peerType;
   for (uint32 i=0; i<_orderedPeersList.GetNumItems(); i++) ret += (i+1)*(_orderedPeersList[i]()->CalculateChecksum());
   if (_peerAttributesBuf()) ret += _peerAttributesBuf()->CalculateChecksum();
   /* deliberately not including _peerAttributesMsg in the checksum since it is redundant with _peerAttributesBuf */
   return ret;
}

uint32 PZGHeartbeatPacket :: FlattenedSizeNotIncludingVariableLengthData() const
{
   return sizeof(uint32)                 // for PZG_HEARTBEAT_PACKET_TYPE_CODE
        + sizeof(_heartbeatPacketID)
        + sizeof(_versionCode)
        + sizeof(_systemKey)
        // _networkSendTimeMicros is deliberately not part of our flattened-size as it will be sent separately for better accuracy
        + sizeof(_tcpAcceptPort)
        + sizeof(_peerUptimeSeconds)
        + ZGPeerID::FlattenedSize()      // for _sourcePeerID
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

void PZGHeartbeatPacket :: Flatten(DataFlattener flat) const
{
   const uint32 opListItemCount = _orderedPeersList.GetNumItems();
   const uint32 attribBufSize   = _peerAttributesBuf() ? _peerAttributesBuf()->GetNumBytes() : 0;

   flat.WriteInt32(PZG_HEARTBEAT_PACKET_TYPE_CODE);
   flat.WriteInt32(_heartbeatPacketID);
   flat.WriteInt32(_versionCode);
   flat.WriteInt64(_systemKey);
   // _networkSendTimeMicros is deliberately not part of our flattened-data as it will be sent separately for better accuracy
   flat.WriteInt16(_tcpAcceptPort);
   flat.WriteInt32(_peerUptimeSeconds);
   flat.WriteFlat(_sourcePeerID);
   flat.WriteInt16(_peerType|(_isFullyAttached?0x8000:0));
   flat.WriteInt16(opListItemCount);  // yes, 16 bits is correct!
   flat.WriteInt16(attribBufSize);    // yes, 16 bits is correct!
   flat.WriteInt16(0); /* reserved for now */
   for (uint32 i=0; i<opListItemCount; i++) flat.WriteFlat(*_orderedPeersList[i]());  // receiver will figure out the lengths from the restored PeerInfo objects
   if (attribBufSize > 0) flat.WriteBytes(*_peerAttributesBuf());
   /** Deliberately not flattening _peerAttributesMsg as it is redundant with _peerAttributesBuf */
}

status_t PZGHeartbeatPacket :: Unflatten(DataUnflattener & unflat)
{
   const uint32 staticBytesNeeded = FlattenedSizeNotIncludingVariableLengthData();
   if (unflat.GetNumBytesAvailable() < staticBytesNeeded)
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::Unflatten():  Packet is too short for static header (" UINT32_FORMAT_SPEC " < " UINT32_FORMAT_SPEC ")\n", unflat.GetNumBytesAvailable(), staticBytesNeeded);
      return B_BAD_DATA;
   }

   const uint32 typeCode = unflat.ReadInt32();
   if (typeCode != PZG_HEARTBEAT_PACKET_TYPE_CODE)
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::Unflatten():  Got unexpected heartbeat typecode " UINT32_FORMAT_SPEC "\n", typeCode);
      return B_BAD_DATA;
   }

   _heartbeatPacketID            = unflat.ReadInt32();
   _versionCode                  = unflat.ReadInt32();
   _systemKey                    = unflat.ReadInt64();
   _networkSendTimeMicros        = 0; // _networkSendTimeMicros is deliberately not part of our unflattened-data as it will be sent separately for better accuracy
   _tcpAcceptPort                = unflat.ReadInt16();
   _peerUptimeSeconds            = unflat.ReadInt32();
   MRETURN_ON_ERROR(unflat.ReadFlat(_sourcePeerID));
   _peerType                     = unflat.ReadInt16();
   _isFullyAttached              = ((_peerType & 0x8000) != 0); _peerType &= ~(0x8000);
   const uint32 opListItemCount  = unflat.ReadInt16();
   const uint32 attribBufSize    = unflat.ReadInt16();
   (void)                          unflat.ReadInt16();   /* skip the currently-unused reserved field, for now */

   _orderedPeersList.Clear();
   MRETURN_ON_ERROR(_orderedPeersList.EnsureSize(opListItemCount));
   for (uint32 i=0; i<opListItemCount; i++)
   {
       PZGHeartbeatPeerInfoRef newPIRef = GetPZGHeartbeatPeerInfoFromPool();
       MRETURN_OOM_ON_NULL(newPIRef());
       MRETURN_ON_ERROR(unflat.ReadFlat(*newPIRef()));
       MRETURN_ON_ERROR(_orderedPeersList.AddTail(newPIRef));
   }
   
   if (attribBufSize > 0) 
   {
      const uint32 numBytesLeft = unflat.GetNumBytesAvailable();
      if (attribBufSize > numBytesLeft)
      {
         LogTime(MUSCLE_LOG_ERROR, "PZGHeartbeatPacket::Unflatten():  attribBufSize too large!  (" UINT32_FORMAT_SPEC " > " UINT32_FORMAT_SPEC ")\n", attribBufSize, numBytesLeft);
         return B_BAD_DATA;
      }
      _peerAttributesBuf = GetByteBufferFromPool(attribBufSize, unflat.GetCurrentReadPointer());
      MRETURN_OOM_ON_NULL(_peerAttributesBuf());
   }
   else _peerAttributesBuf.Reset();

   _peerAttributesMsg.Reset();  // this can be demand-allocated later, if necessary

   return unflat.GetStatus();
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
   muscleSprintf(buf, "Heartbeat:  PacketID=" UINT32_FORMAT_SPEC " cversion=[%s] sysKey=" UINT64_FORMAT_SPEC " netSendTime=" UINT64_FORMAT_SPEC " tcpPort=%u peerType=%u isFullyAttached=%i uptimeSeconds=" UINT32_FORMAT_SPEC " sourcePeerID=[%s] attrSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC, _heartbeatPacketID, CompatibilityVersionCodeToString(_versionCode)(), _systemKey, _networkSendTimeMicros, _tcpAcceptPort, _peerType, _isFullyAttached, _peerUptimeSeconds, _sourcePeerID.ToString()(), _peerAttributesBuf()?_peerAttributesBuf()->GetNumBytes():666, _peerAttributesBuf()?_peerAttributesBuf()->CalculateChecksum():666);

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

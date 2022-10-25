#include "zlib/ZLibUtilityFunctions.h"
#include "zg/private/PZGDatabaseUpdate.h"

namespace zg_private
{

static PZGDatabaseUpdateRef::ItemPool _dbUpdatePool;

PZGDatabaseUpdateRef GetPZGDatabaseUpdateFromPool(uint8 updateType, uint16 databaseIndex, uint64 updateID, const ZGPeerID & sourcePeerID, uint32 preUpdateDBChecksum)
{
   PZGDatabaseUpdateRef ret(_dbUpdatePool.ObtainObject());
   if (ret())
   {
      ret()->SetUpdateType(updateType);
      ret()->SetDatabaseIndex(databaseIndex);
      ret()->SetUpdateID(updateID);
      ret()->SetSourcePeerID(sourcePeerID);
      ret()->SetPreUpdateDBChecksum(preUpdateDBChecksum);
   }
   return ret;
}

PZGDatabaseUpdateRef GetPZGDatabaseUpdateFromPool()
{
   return PZGDatabaseUpdateRef(_dbUpdatePool.ObtainObject());
}

PZGDatabaseUpdate :: PZGDatabaseUpdate()
   : _updateType(PZG_DATABASE_UPDATE_TYPE_NOOP)
   , _databaseIndex(0)
   , _seniorElapsedTimeMillis(0)
   , _seniorStartTimeMicros(0)
   , _updateID(0)
   , _preUpdateDBChecksum(0)
   , _postUpdateDBChecksum(0)
{
   // empty
}

PZGDatabaseUpdate :: PZGDatabaseUpdate(const PZGDatabaseUpdate & rhs)
   : _updateType(rhs._updateType)
   , _databaseIndex(rhs._databaseIndex)
   , _seniorElapsedTimeMillis(rhs._seniorElapsedTimeMillis)
   , _seniorStartTimeMicros(rhs._seniorStartTimeMicros)
   , _sourcePeerID(rhs._sourcePeerID)
   , _updateID(rhs._updateID)
   , _preUpdateDBChecksum(rhs._preUpdateDBChecksum)
   , _postUpdateDBChecksum(rhs._postUpdateDBChecksum)
   , _updateBuf(rhs._updateBuf)
   , _updateMsg(rhs._updateMsg)
{
   // empty
}

PZGDatabaseUpdate & PZGDatabaseUpdate :: operator=(const PZGDatabaseUpdate & rhs)
{
   _updateType              = rhs._updateType;
   _databaseIndex           = rhs._databaseIndex;
   _seniorElapsedTimeMillis = rhs._seniorElapsedTimeMillis;
   _seniorStartTimeMicros   = rhs._seniorStartTimeMicros;
   _sourcePeerID            = rhs._sourcePeerID;
   _updateID                = rhs._updateID;
   _preUpdateDBChecksum     = rhs._preUpdateDBChecksum;
   _postUpdateDBChecksum    = rhs._postUpdateDBChecksum;
   _updateBuf               = rhs._updateBuf;
   _updateMsg               = rhs._updateMsg;
   return *this;
}

uint32 PZGDatabaseUpdate :: CalculateChecksum() const
{
   uint32 ret = ((uint32)_updateType) + ((uint32)_databaseIndex) + ((uint32)_seniorElapsedTimeMillis) + CalculateChecksumForUint64(_seniorStartTimeMicros) + _sourcePeerID.CalculateChecksum() + CalculateChecksumForUint64(_updateID) + _preUpdateDBChecksum + (_postUpdateDBChecksum*3);
   const ConstByteBufferRef & updateBuf = GetPayloadBuffer();  // we're deliberately using the buffer version here, not the Message version
   if (updateBuf()) ret += updateBuf()->CalculateChecksum();
   return ret;
}

uint32 PZGDatabaseUpdate :: FlattenedSize() const
{
   /** Note that the payload Message is deliberately NOT part of the FlattenedSize! */
   const ConstByteBufferRef & updateBuf = GetPayloadBuffer();
   return FlattenedSizeNotIncludingPayload() + (updateBuf() ? updateBuf()->FlattenedSize() : 0);
}

uint32 PZGDatabaseUpdate :: FlattenedSizeNotIncludingPayload() const
{
   return sizeof(uint32)                   + /* will be the PZG_DATABASE_UPDATE_TYPE_CODE header */
          sizeof(_updateType)              +
          sizeof(uint8)                    + /* this is a reserved byte for now */
          sizeof(_databaseIndex)           +
          sizeof(_seniorElapsedTimeMillis) +
          sizeof(_seniorStartTimeMicros)   +
          sizeof(uint16)                   + /* this is a reserved word for now */
          _sourcePeerID.FlattenedSize()    +
          sizeof(_updateID)                +
          sizeof(_preUpdateDBChecksum)     +
          sizeof(_postUpdateDBChecksum)    +
          sizeof(uint32)                   + /* this will be this object's checksum */
          sizeof(uint32)                   ; /* this will be updateBuf.FlattenedSize() */
}

void PZGDatabaseUpdate :: Flatten(DataFlattener flat) const
{
   flat.WriteInt32(PZG_DATABASE_UPDATE_TYPE_CODE);
   flat.WriteInt8(_updateType);
   flat.WriteInt8(0);  /* this field is reserved for now */
   flat.WriteInt16(_databaseIndex);
   flat.WriteInt16(_seniorElapsedTimeMillis);
   flat.WriteInt16(0);  /* this field is reserved */
   flat.WriteInt64(_seniorStartTimeMicros);
   flat.WriteFlat(_sourcePeerID);
   flat.WriteInt64(_updateID);
   flat.WriteInt32(_preUpdateDBChecksum);
   flat.WriteInt32(_postUpdateDBChecksum);
   flat.WriteInt32(CalculateChecksum());

   const ConstByteBufferRef & updateBuf = GetPayloadBuffer();
   flat.WriteInt32(updateBuf() ? updateBuf()->GetNumBytes() : 0);
   if (updateBuf()) flat.WriteBytes(*updateBuf());

   /** Note that _updateMsg is deliberately NOT part of the flattened data! */
}

status_t PZGDatabaseUpdate :: Unflatten(DataUnflattener & unflat)
{
   const uint32 typeCode = unflat.ReadInt32();
   if (typeCode != PZG_DATABASE_UPDATE_TYPE_CODE)
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdate::Unflatten():  Got unexpected update typecode " UINT32_FORMAT_SPEC "\n", typeCode);
      return B_BAD_DATA;
   }

   _updateBuf.Reset();
   _updateMsg.Reset();

   _updateType                          = unflat.ReadInt8();
   (void)                                 unflat.ReadInt8();   // skip the reserved/padding byte
   _databaseIndex                       = unflat.ReadInt16();
   _seniorElapsedTimeMillis             = unflat.ReadInt16();
   (void)                                 unflat.ReadInt16();  // reserved 16-bit field is here; maybe we'll do something with it someday
   _seniorStartTimeMicros               = unflat.ReadInt64();
   MRETURN_ON_ERROR(unflat.ReadFlat(_sourcePeerID));
   _updateID                            = unflat.ReadInt64();
   _preUpdateDBChecksum                 = unflat.ReadInt32();
   _postUpdateDBChecksum                = unflat.ReadInt32();
   const uint32 chk                     = unflat.ReadInt32();
   const uint32 dataSize                = unflat.ReadInt32();
   if (unflat.GetNumBytesAvailable() < dataSize) return B_BAD_DATA;  // truncated buffer, oh no!

   if (dataSize > 0)  // 0 data is taken to mean a NULL/empty buffer (we don't distinguish between the two)
   {
      _updateBuf = GetByteBufferFromPool(dataSize, unflat.GetCurrentReadPointer());
      MRETURN_OOM_ON_NULL(_updateBuf());
   }

   const uint32 myChk = CalculateChecksum();
   if (chk != myChk)
   {
      LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdate::Unflatten():  Bad checksum!  Expected " UINT32_FORMAT_SPEC", got " UINT32_FORMAT_SPEC "\n", myChk, chk);
      return B_BAD_DATA;
   }

   return unflat.GetStatus();
}

String PZGDatabaseUpdate :: ToString() const
{
   char buf[512];
   muscleSprintf(buf, "UpdateID=" UINT64_FORMAT_SPEC " Type=%u db=%u elapsed=%umS seniorTime=" UINT64_FORMAT_SPEC " sourcePeerID=%s preChk=" UINT32_FORMAT_SPEC " postChk=" UINT32_FORMAT_SPEC " _updateBuf=" INT32_FORMAT_SPEC " _updateMsg=" INT32_FORMAT_SPEC, _updateID, _updateType, _databaseIndex, _seniorElapsedTimeMillis, _seniorStartTimeMicros, _sourcePeerID.ToString()(), _preUpdateDBChecksum, _postUpdateDBChecksum, _updateBuf()?_updateBuf()->GetNumBytes():0, _updateMsg()?_updateMsg()->FlattenedSize():0);
   return buf;
}

void PZGDatabaseUpdate :: PrintToStream() const
{
   puts(ToString()());
}

status_t PZGDatabaseUpdate :: CopyFromImplementation(const Flattenable & copyFrom)
{
   const PZGDatabaseUpdate * p = dynamic_cast<const PZGDatabaseUpdate *>(&copyFrom);
   if (p) {*this = *p; return B_NO_ERROR;}
     else return FlatCountable::CopyFromImplementation(copyFrom);
}

void PZGDatabaseUpdate :: UncachePayloadBufferAsMessage() const
{
   // we don't think we'll be needing the Message version again anytime soon, so we'll free up its memory
   // ... but only if we still have a flattened version.  (we're not savages)
   if (_updateBuf()) _updateMsg.Reset();
}

const ConstMessageRef & PZGDatabaseUpdate :: GetPayloadBufferAsMessage() const
{
   if (_updateMsg()) return _updateMsg;  // re-use the prevously-constructed Message, if we have one
   if (_updateBuf()) _updateMsg = GetMessageFromPool(InflateByteBuffer(_updateBuf));  // demand-calculate and cache one if we don't
   return _updateMsg;
}

const ConstByteBufferRef & PZGDatabaseUpdate :: GetPayloadBuffer() const
{
   if (_updateBuf()) return _updateBuf;  // re-use the prevously-constructed buffer, if we have one
   if (_updateMsg()) _updateBuf = DeflateByteBuffer(_updateMsg()->FlattenToByteBuffer(), 9);  // demand-calculate and cache one if we don't
   return _updateBuf;
}

void PZGDatabaseUpdate :: SetPayloadMessage(const ConstMessageRef & updateMsg)
{
   _updateBuf.Reset();  // this may be demand-calculated later; for now make sure we dump any now-inappropriate older version
   _updateMsg = updateMsg;
}

};  // end namespace zg_private

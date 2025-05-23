#ifndef PZGDatabaseUpdate_h
#define PZGDatabaseUpdate_h

#include "zg/private/PZGHeartbeatSettings.h"
#include "zg/private/PZGDatabaseStateInfo.h"

namespace zg_private
{

enum {PZG_DATABASE_UPDATE_TYPE_CODE = 1684174192}; // 'dbup'

enum {
   PZG_DATABASE_UPDATE_TYPE_NOOP = 0, // Doesn't do anything to the database at all
   PZG_DATABASE_UPDATE_TYPE_RESET,    // resets the database's state to its well-known default state
   PZG_DATABASE_UPDATE_TYPE_REPLACE,  // fully replaces the database's state with the state contained in the attached data
   PZG_DATABASE_UPDATE_TYPE_UPDATE,   // uses the attached data to incrementally update the database's state
   NUM_PZG_DATABASE_UPDATE_TYPES,     // guard value
};

/** This class represents an update (full or incremental) to an existing database. */
class PZGDatabaseUpdate : public FlatCountable
{
public:
   PZGDatabaseUpdate();
   PZGDatabaseUpdate(const PZGDatabaseUpdate & rhs);

   PZGDatabaseUpdate & operator=(const PZGDatabaseUpdate & rhs);

   MUSCLE_NODISCARD virtual bool IsFixedSize() const {return false;}
   MUSCLE_NODISCARD virtual uint32 TypeCode() const {return PZG_DATABASE_UPDATE_TYPE_CODE;}
   MUSCLE_NODISCARD virtual uint32 FlattenedSize() const;
   virtual void Flatten(DataFlattener flat) const;
   virtual status_t Unflatten(DataUnflattener & unflat);

   void Print(const OutputPrinter & p) const;
   MUSCLE_NODISCARD String ToString() const;

   MUSCLE_NODISCARD uint8 GetUpdateType()               const {return _updateType;}
   MUSCLE_NODISCARD uint16 GetDatabaseIndex()           const {return _databaseIndex;}
   MUSCLE_NODISCARD uint16 GetSeniorElapsedTimeMillis() const {return _seniorElapsedTimeMillis;}
   MUSCLE_NODISCARD uint64 GetSeniorStartTimeMicros()   const {return _seniorStartTimeMicros;}
   MUSCLE_NODISCARD const ZGPeerID & GetSourcePeerID()  const {return _sourcePeerID;}
   MUSCLE_NODISCARD uint64 GetUpdateID()                const {return _updateID;}
   MUSCLE_NODISCARD uint32 GetPreUpdateDBChecksum()     const {return _preUpdateDBChecksum;}
   MUSCLE_NODISCARD uint32 GetPostUpdateDBChecksum()    const {return _postUpdateDBChecksum;}

   MUSCLE_NODISCARD const ConstMessageRef & GetPayloadBufferAsMessage() const;
   MUSCLE_NODISCARD const ConstByteBufferRef & GetPayloadBuffer() const;

   void SetUpdateType(uint8 updateType)                {_updateType              = updateType;}
   void SetDatabaseIndex(uint16 databaseIndex)         {_databaseIndex           = databaseIndex;}
   void SetSeniorStartTimeMicros(uint64 micros)        {_seniorStartTimeMicros   = micros;}  // expressed as a timestamp of the GetNetworkTime64() clock
   void SetSeniorElapsedTimeMicros(uint64 micros)      {_seniorElapsedTimeMillis = (uint16) muscleMin((uint64)65535, (uint64) MicrosToMillis(micros));}
   void SetSourcePeerID(const ZGPeerID & peerID)       {_sourcePeerID            = peerID;}
   void SetUpdateID(uint64 updateID)                   {_updateID                = updateID;}
   void SetPreUpdateDBChecksum(uint32 preDBChecksum)   {_preUpdateDBChecksum     = preDBChecksum;}
   void SetSeniorElapsedTimeMillis(uint16 millis)      {_seniorElapsedTimeMillis = millis;}
   void SetPostUpdateDBChecksum(uint32 postDBChecksum) {_postUpdateDBChecksum    = postDBChecksum;}
   void SetPayloadMessage(const ConstMessageRef & payloadMsg);
   void UncachePayloadBufferAsMessage() const;

   MUSCLE_NODISCARD uint32 CalculateChecksum() const;

protected:
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);

private:
   MUSCLE_NODISCARD uint32 FlattenedSizeNotIncludingPayload() const;

   uint8 _updateType;                 // PZG_DATABASE_UPDATE_TYPE_*
   uint16 _databaseIndex;             // Index of the database (within this replicated-database-arena) that this update is intended for
   uint16 _seniorElapsedTimeMillis;   // how many milliseconds it took to execute this update on the senior peer
   uint64 _seniorStartTimeMicros;     // when SeniorUpdated() started executing on the senior peer, expressed as a timestamp of the GetNetworkTime64() clock
   ZGPeerID _sourcePeerID;            // ID of the peer that requested this update
   uint64 _updateID;                  // State-ID that this update will place the database into when applied.
   uint32 _preUpdateDBChecksum;       // 32-bit checksum of our database as it was before this update was applied
   uint32 _postUpdateDBChecksum;      // 32-bit checksum of our database as it was after this update was applied

   mutable ConstByteBufferRef _updateBuf; // demand-allocated from _updateMsg
   mutable ConstMessageRef _updateMsg;    // demand-allocated from _updateBuf
};
DECLARE_REFTYPES(PZGDatabaseUpdate);

/** Returns a reference to a newly allocated PZGDatabaseUpdate object */
PZGDatabaseUpdateRef GetPZGDatabaseUpdateFromPool();

/** Returns a partially-populated reference to a newly allocated PZGDatabaseUpdate object */
PZGDatabaseUpdateRef GetPZGDatabaseUpdateFromPool(uint8 updateType, uint16 databaseIndex, uint64 updateID, const ZGPeerID & sourcePeerID, uint32 preUpdateDBChecksum);

};  // end namespace zg_private

#endif

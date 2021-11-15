#ifndef PZGUpdateBackOrderKey_h
#define PZGUpdateBackOrderKey_h

#include "support/PseudoFlattenable.h"
#include "zg/ZGPeerID.h"

namespace zg_private
{

enum {PZG_UPDATE_BACKORDER_KEY_TYPE = 1969385323}; /**< 'ubok' -- the type code of the PZGUpdateBackOrderKey class */

/** This key represents a request to the senior peer to resend a PZGDatabaseUpdate to us, since we need it and don't have it.
  * That way the junior peer can keep track of what it has on order so as not to send orders for a given update more than once.
  */
class PZGUpdateBackOrderKey : public PseudoFlattenable
{
public:
   PZGUpdateBackOrderKey() : _whichDatabase(0), _updateID(0) {/* empty */}
   PZGUpdateBackOrderKey(const ZGPeerID & targetPeerID, uint32 whichDatabase, uint64 updateID) : _targetPeerID(targetPeerID), _whichDatabase(whichDatabase), _updateID(updateID) {/* empty */}

   const ZGPeerID & GetTargetPeerID() const {return _targetPeerID;}
   uint32 GetDatabaseIndex() const {return _whichDatabase;}
   uint64 GetDatabaseUpdateID() const {return _updateID;}

   bool operator == (const PZGUpdateBackOrderKey & rhs) const {return ((_targetPeerID == rhs._targetPeerID)&&(_whichDatabase == rhs._whichDatabase)&&(_updateID == rhs._updateID));}
   bool operator != (const PZGUpdateBackOrderKey & rhs) const {return !(*this==rhs);}

   uint32 HashCode() const {return _targetPeerID.HashCode()+(_whichDatabase*333)+CalculateHashCode(_updateID);}
   String ToString() const {return String("UBOK:  [%1] db=%2 updateID=%3").Arg(_targetPeerID).Arg(_whichDatabase).Arg(_updateID);}

   static MUSCLE_CONSTEXPR bool IsFixedSize()             {return true;}
   static MUSCLE_CONSTEXPR uint32 TypeCode()              {return PZG_UPDATE_BACKORDER_KEY_TYPE;}
   static MUSCLE_CONSTEXPR bool AllowsTypeCode(uint32 tc) {return (TypeCode()==tc);}
   static MUSCLE_CONSTEXPR uint32 FlattenedSize()         {return ZGPeerID::FlattenedSize() + sizeof(_whichDatabase) + sizeof(_updateID);}

   void Flatten(uint8 * buffer) const
   {
      _targetPeerID.Flatten(buffer);                                  buffer += ZGPeerID::FlattenedSize();
      muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(_whichDatabase)); buffer += sizeof(_whichDatabase);
      muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT64(_updateID));      //buffer += sizeof(_updateID);
   }

   status_t Unflatten(const uint8 * buffer, uint32 size)
   {
      if (size >= FlattenedSize())
      {
         MRETURN_ON_ERROR(_targetPeerID.Unflatten(buffer, size));                buffer += ZGPeerID::FlattenedSize();
         _whichDatabase = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer)); buffer += sizeof(_whichDatabase);
         _updateID      = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(buffer)); //buffer += sizeof(_updateID);
         return B_NO_ERROR;
      }
      else return B_BAD_DATA;
   }

private:
   ZGPeerID _targetPeerID;
   uint32 _whichDatabase;
   uint64 _updateID;
};

};  // end namespace zg_private

#endif

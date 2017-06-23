#ifndef PZGUpdateBackOrderKey_h
#define PZGUpdateBackOrderKey_h

#include "zg/ZGPeerID.h"

namespace zg_private
{

/** This key represents a request to the senior peer to resend a PZGDatabaseUpdate to us, since we need it and don't have it.
  * That way the junior peer can keep track of what it has on order so as not to send orders for a given update more than once.
  */
class PZGUpdateBackOrderKey
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
  
private:
   ZGPeerID _targetPeerID;
   uint32 _whichDatabase;
   uint64 _updateID;
};

};  // end namespace zg_private

#endif

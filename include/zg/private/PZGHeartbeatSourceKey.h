#ifndef PZGHeartbeatSourceKey_h
#define PZGHeartbeatSourceKey_h

#include "zg/ZGPeerID.h"
#include "util/NetworkUtilityFunctions.h"

namespace zg_private
{

/** This key represents a single source of incoming heartbeat packets.
  */
class PZGHeartbeatSourceKey
{
public:
   PZGHeartbeatSourceKey() {/* empty */}
   PZGHeartbeatSourceKey(const IPAddressAndPort & source, const ZGPeerID & peerID) : _source(source), _peerID(peerID) {/* empty */}

   const IPAddressAndPort & GetIPAddressAndPort() const {return _source;}
   const ZGPeerID & GetPeerID() const {return _peerID;}

   bool IsValid() const {return ((_source.IsValid())&&(_peerID.IsValid()));}

   bool operator == (const PZGHeartbeatSourceKey & rhs) const {return ((_source == rhs._source)&&(_peerID == rhs._peerID));}
   bool operator != (const PZGHeartbeatSourceKey & rhs) const {return !(*this==rhs);}

   bool operator <  (const PZGHeartbeatSourceKey & rhs) const {return ((_source < rhs._source)||((_source == rhs._source)&&(_peerID < rhs._peerID)));}
   bool operator >= (const PZGHeartbeatSourceKey & rhs) const {return !(*this<rhs);}
   bool operator >  (const PZGHeartbeatSourceKey & rhs) const {return ((_source > rhs._source)||((_source == rhs._source)&&(_peerID > rhs._peerID)));}
   bool operator <= (const PZGHeartbeatSourceKey & rhs) const {return !(*this>rhs);}

   uint32 HashCode() const {return _peerID.HashCode()+_source.HashCode();}

   String ToString() const {return String("{%1 -> %2}").Arg(_source.ToString()).Arg(_peerID.ToString());}
  
private:
   IPAddressAndPort _source;
   ZGPeerID _peerID;
};

};  // end namespace zg_private

#endif

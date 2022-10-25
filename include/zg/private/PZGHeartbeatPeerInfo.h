#ifndef PZGHeartbeatPeerInfo_h
#define PZGHeartbeatPeerInfo_h

#include "util/FlatCountable.h"
#include "zg/ZGPeerID.h"
#include "zg/private/PZGNameSpace.h"

namespace zg_private
{

enum {PZG_HEARTBEAT_PEER_INFO_TYPE_CODE = 2053597282}; // 'zghb'

/** This class represents some of the data included in a PZGHeartbeatPacket
  * This is the data within the packet that pertains to a particular peer.
  */
class PZGHeartbeatPeerInfo : public FlatCountable
{
public:
   PZGHeartbeatPeerInfo() {/* empty */}
   virtual ~PZGHeartbeatPeerInfo() {/* empty */}

   virtual bool IsFixedSize() const {return false;}
   virtual uint32 TypeCode() const {return PZG_HEARTBEAT_PEER_INFO_TYPE_CODE;}
   virtual uint32 FlattenedSize() const;

   virtual void Flatten(DataFlattener flat) const;
   status_t Unflatten(DataUnflattener & unflat);

   void PrintToStream() const;
   String ToString() const;

   uint32 CalculateChecksum() const;

   void SetPeerID(const ZGPeerID & id) {_peerID = id;}
   const ZGPeerID & GetPeerID() const {return _peerID;}

   status_t PutTimingInfo(uint16 srcTag, uint32 sourceHeartbeatPacketID, uint32 dwellTimeMicros);

   /** This class contains timing information for a specified network interface within this peer */
   class PZGTimingInfo : public PseudoFlattenable
   {
   public:
      /** Default Constructor */
      PZGTimingInfo() : _sourceTag(0), _heartbeatPacketID(0), _dwellTimeMicros(0) {/* empty */}

      /** Constructor
        * @param sourceTag an ID that is meaningful to the receiver of this packet, regarding which interface this packet is about
        * @param sourceHeartbeatPacketID The heartbeat-packet-ID of the last heartbeat packet we received from that peer
        * @param dwellTimeMicros How many microseconds it has been since we received that packet, or ((uint32)-1) if that info isn't available.
        */
      PZGTimingInfo(uint16 sourceTag, uint32 sourceHeartbeatPacketID, uint32 dwellTimeMicros) : _sourceTag(sourceTag), _heartbeatPacketID(sourceHeartbeatPacketID), _dwellTimeMicros(dwellTimeMicros) {/* empty */}

      static MUSCLE_CONSTEXPR bool IsFixedSize() {return true;}
      static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return sizeof(uint16)+sizeof(uint16)+sizeof(uint32)+sizeof(uint32);}  // the second uint16 is just reserved/padding for now

      void Flatten(DataFlattener flat) const;
      status_t Unflatten(DataUnflattener & unflat);

      uint32 CalculateChecksum() const;

      String ToString() const;

      uint16 GetSourceTag() const {return _sourceTag;}
      uint32 GetSourceHeartbeatPacketID() const {return _heartbeatPacketID;}
      uint32 GetDwellTimeMicros() const {return _dwellTimeMicros;}

   private:
      uint16 _sourceTag;
      uint32 _heartbeatPacketID;
      uint32 _dwellTimeMicros;
   };

   const Queue<PZGTimingInfo> & GetTimingInfos() const {return _timings;}

private:
   ZGPeerID _peerID;
   Queue<PZGTimingInfo> _timings;
};
DECLARE_REFTYPES(PZGHeartbeatPeerInfo);

/** Returns a reference to a new PZGHeartbeatPeerInfo object to use */
PZGHeartbeatPeerInfoRef GetPZGHeartbeatPeerInfoFromPool();

};  // end namespace zg_private

#endif

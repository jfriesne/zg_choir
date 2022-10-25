#ifndef PZGHeartbeatPacket_h
#define PZGHeartbeatPacket_h

#include "zg/private/PZGHeartbeatSettings.h"
#include "zg/private/PZGHeartbeatPeerInfo.h"

namespace zg_private
{

enum {PZG_HEARTBEAT_PACKET_TYPE_CODE = 2053597282}; // 'zghb'

/** This class represents a single heartbeat-packet.  Heartbeat packets are sent out periodically by all peers,
  * so the other peers can know of their existence and status.  If no heartbeats are received from a peer for an
  * extended period, that peer is assumed to have gone away.
  */
class PZGHeartbeatPacket : public FlatCountable
{
public:
   PZGHeartbeatPacket();
   PZGHeartbeatPacket(const PZGHeartbeatSettings & hbSettings, uint32 uptimeSeconds, bool isFullyAttached, uint32 packetID);

   void Initialize(const PZGHeartbeatSettings & hbSettings, uint32 uptimeSeconds, bool isFullyAttached, uint32 packetID);

   virtual bool IsFixedSize() const {return false;}
   virtual uint32 TypeCode() const {return PZG_HEARTBEAT_PACKET_TYPE_CODE;}
   virtual uint32 FlattenedSize() const;
   virtual void Flatten(DataFlattener flat) const;
   virtual status_t Unflatten(DataUnflattener & unflat);

   void PrintToStream() const;
   String ToString() const;

   uint32 CalculateChecksum() const;

   bool IsFullyAttached()        const {return _isFullyAttached;}
   uint32 GetHeartbeatPacketID() const {return _heartbeatPacketID;}
   uint32 GetVersionCode()       const {return _versionCode;}
   uint64 GetSystemKey()         const {return _systemKey;}
   uint16 GetTCPAcceptPort()     const {return _tcpAcceptPort;}
   uint16 GetPeerType()          const {return _peerType;}
   uint32 GetPeerUptimeSeconds() const {return _peerUptimeSeconds;}
   const ZGPeerID & GetSourcePeerID() const {return _sourcePeerID;}

   const Queue<ConstPZGHeartbeatPeerInfoRef> & GetOrderedPeersList() const {return _orderedPeersList;}
         Queue<ConstPZGHeartbeatPeerInfoRef> & GetOrderedPeersList()       {return _orderedPeersList;}

   ConstMessageRef GetPeerAttributesAsMessage() const;

   // The current network-time (according to the sender) at the moment this packet was sent.
   // note that this value is NOT flattened/unflattened along with the rest of the data in this object.
   // That is because we want to send it separately for better accuracy (otherwise we'd have to place
   // a value into this object before we zlib-compressed the heartbeat data, and the zlib-compression
   // time would get added in to the network-delay measurement)
   void SetNetworkSendTimeMicros(uint64 networkSendTimeMicros) {_networkSendTimeMicros = networkSendTimeMicros;}
   uint64 GetNetworkSendTimeMicros() const {return _networkSendTimeMicros;}

   bool IsEqualIgnoreTransients(const PZGHeartbeatPacket & rhs) const;

protected:
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);

private:
   uint32 FlattenedSizeNotIncludingVariableLengthData() const;

   uint32 _heartbeatPacketID;
   uint32 _versionCode;
   uint64 _systemKey;             // hash-code of our signature and system name, just for sanity checking that we aren't getting crosstalk or random packets
   uint64 _networkSendTimeMicros; // time at which this packet was sent, according to the sender's network-clock
   uint16 _tcpAcceptPort;
   uint16 _peerType;
   uint32 _peerUptimeSeconds;
   ZGPeerID _sourcePeerID;
   Queue<ConstPZGHeartbeatPeerInfoRef> _orderedPeersList;
   bool _isFullyAttached;

   ConstByteBufferRef _peerAttributesBuf; // flattened version of _peerAttributesMsg
   mutable MessageRef _peerAttributesMsg; // unflattened version of _peerAttributesBuf (demand-allocated)
};
DECLARE_REFTYPES(PZGHeartbeatPacket);

/** Same as a PZGHeartbeatPacket but also adds some additional fields that the
  * receiver can use to keep track of some state information that is not
  * meant to be sent across the network.
  */
class PZGHeartbeatPacketWithMetaData : public PZGHeartbeatPacket
{
public:
   PZGHeartbeatPacketWithMetaData();
   PZGHeartbeatPacketWithMetaData(const PZGHeartbeatSettings & hbSettings, uint32 uptimeSeconds, bool isFullyAttached, uint32 packetID);

   // Time at which we received this packet (as reported by GetRunTime64())
   uint64 GetLocalReceiveTimeMicros() const {return _localReceiveTimeMicros;}
   void SetLocalReceiveTimeMicros(uint64 rTime) {_localReceiveTimeMicros = rTime;};

   const IPAddressAndPort & GetPacketSource() const {return _packetSource;}
   uint16 GetSourceTag() const {return _sourceTag;}
   void SetPacketSource(const IPAddressAndPort & source, uint16 sourceTag) {_packetSource = source; _sourceTag = sourceTag;}

   void SetHaveSentTimingReply(bool haveSent) {_haveSentTimingReply = haveSent;}
   bool HaveSentTimingReply() const {return _haveSentTimingReply;}

private:
   uint64 _localReceiveTimeMicros;  // time at which this packet was received, according to the receiver's local monotonic clock
   IPAddressAndPort _packetSource;  // IP address that this packet was received from.
   uint16 _sourceTag;               // a value sent to us with the packet, given a short per-sender-unique ID to the interface
   bool _haveSentTimingReply;       // set this to true when we send out timing data using this object, to avoid doing so more than once
};
DECLARE_REFTYPES(PZGHeartbeatPacketWithMetaData);

};  // end namespace zg_private

#endif

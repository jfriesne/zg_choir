#include <atomic>

#include "util/NetworkUtilityFunctions.h"
#include "zlib/ZLibCodec.h"

#include "zg/ZGConstants.h"
#include "zg/INetworkTimeProvider.h"
#include "zg/private/PZGConstants.h"
#include "zg/private/PZGHeartbeatPacket.h"
#include "zg/private/PZGHeartbeatSourceKey.h"
#include "zg/private/PZGHeartbeatSourceState.h"

namespace zg_private
{

class PZGHeartbeatSession;
class ComparePeerIDsBySeniorityFunctor;

/** This class contains the internal state machine for the heartbeat thread. */
class PZGHeartbeatThreadState : public INetworkTimeProvider
{
public:
   PZGHeartbeatThreadState();
   virtual ~PZGHeartbeatThreadState();

   void Initialize(const ConstPZGHeartbeatSettingsRef & hbSettings, uint64 startTime);
   MUSCLE_NODISCARD uint64 GetPulseTime() const;
   void Pulse(Queue<MessageRef> & messagesForOwnerThread);
   void MessageReceivedFromOwner(const MessageRef & msgFromOwner);
   void ReceiveMulticastTraffic(PacketDataIO & dio);

   MUSCLE_NODISCARD int64 MainThreadGetToNetworkTimeOffset() const {return _mainThreadToNetworkTimeOffset;} // this will be called from the main thread

   MUSCLE_NODISCARD uint64 GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const;

   // INetworkTimeProvider interface
   MUSCLE_NODISCARD virtual uint64 GetNetworkTime64() const {return IsFullyAttached() ? GetNetworkTime64ForRunTime64(GetRunTime64()) : 0;}
   MUSCLE_NODISCARD virtual uint64 GetRunTime64ForNetworkTime64(uint64 networkTime64TimeStamp) const {return ((_toNetworkTimeOffset==INVALID_TIME_OFFSET)||(networkTime64TimeStamp==MUSCLE_TIME_NEVER))?MUSCLE_TIME_NEVER:(networkTime64TimeStamp-_toNetworkTimeOffset);}
   MUSCLE_NODISCARD virtual uint64 GetNetworkTime64ForRunTime64(uint64 runTime64TimeStamp) const {return ((_toNetworkTimeOffset==INVALID_TIME_OFFSET)||(runTime64TimeStamp==MUSCLE_TIME_NEVER))?MUSCLE_TIME_NEVER:(runTime64TimeStamp+_toNetworkTimeOffset);}
   MUSCLE_NODISCARD virtual int64 GetToNetworkTimeOffset() const {return _toNetworkTimeOffset;}

private:
   friend class PZGHeartbeatSession;

   MUSCLE_NODISCARD bool PeersListMatchesIgnoreOrdering(const Queue<ConstPZGHeartbeatPeerInfoRef> & infoQ) const;
   MUSCLE_NODISCARD ZGPeerID GetKingmakerPeerID() const;
   MUSCLE_NODISCARD PZGHeartbeatSourceKey GetKingmakerPeerSource() const;
   MUSCLE_NODISCARD Queue<ZGPeerID> CalculateOrderedPeersList();
   ConstPZGHeartbeatPeerInfoRef GetPZGHeartbeatPeerInfoRefFor(uint64 now, const ZGPeerID & peerID) const;
   void ScheduleUpdateOfficialPeersList(bool forceUpdate) {_updateOfficialPeersListPending = true; if (forceUpdate) _forceOfficialPeersUpdate = true;}
   void ScheduleUpdateToNetworkTimeOffset() {_updateToNetworkTimeOffsetPending = true;}
   void UpdateToNetworkTimeOffset();
   status_t SendHeartbeatPackets();
   MUSCLE_NODISCARD const ZGPeerID & GetSeniorPeerID() const {return _lastSourcesSentToMaster.GetFirstKeyWithDefault().GetPeerID();}
   MUSCLE_NODISCARD bool IAmTheSeniorPeer() const {return GetSeniorPeerID() == _hbSettings()->GetLocalPeerID();}
   MessageRef UpdateOfficialPeersList(bool forceUpdate);
   MUSCLE_NODISCARD bool IsAtLeastHalfAttached() const {return (_now >= _halfAttachedTime);}
   MUSCLE_NODISCARD bool IsFullyAttached()       const {return (_now >= _fullyAttachedTime);}

   PZGHeartbeatPacketWithMetaDataRef ParseHeartbeatPacketBuffer(const ByteBuffer & defBuf, const IPAddressAndPort & sourceIAP, uint64 localReceiveTimeMicros);
   void IntroduceSource(const PZGHeartbeatSourceKey & source, const PZGHeartbeatPacketWithMetaDataRef & newHB, uint64 localExpirationTimeMicros);
   void ExpireSource(const PZGHeartbeatSourceKey & source);

   void PrintTimeSynchronizationDeltas() const;
   void EnsureHeartbeatSourceTagsTableUpdated();

   friend class ComparePeerIDsBySeniorityFunctor;
   MUSCLE_NODISCARD uint16 GetPeerTypeFromQueue(const ZGPeerID & pid, const Queue<IPAddressAndPort> & q) const;
   MUSCLE_NODISCARD uint32 GetPeerUptimeSecondsFromQueue(const ZGPeerID & pid, const Queue<IPAddressAndPort> & q) const;
   MUSCLE_NODISCARD int ComparePeerIDsBySeniority(const ZGPeerID & pid1, const ZGPeerID & pid2) const;

   ConstPZGHeartbeatSettingsRef _hbSettings;

   uint64 _heartbeatThreadStateBirthdate;
   uint64 _heartbeatPingInterval;
   uint64 _heartbeatExpirationTimeMicros;
   uint64 _nextSendHeartbeatTime;
   uint64 _now;   // set before our callbacks are called
   uint64 _halfAttachedTime;
   uint64 _fullyAttachedTime;
   bool _fullAttachmentReported;

   int64 _toNetworkTimeOffset;  // microseconds we need to add to our GetRunTime64() value to get the current network time
   std::atomic<int64> _mainThreadToNetworkTimeOffset;  // this is the same as _toNetworkTimeOffset except safe for the main thread to read atomically
   bool _updateToNetworkTimeOffsetPending;

   Queue<PacketDataIORef> _multicastDataIOs;
   bool _recreateMulticastDataIOsRequested;

   ByteBuffer _rawScratchBuf;
   ByteBuffer _deflatedScratchBuf;
   Hashtable<uint32, uint64> _recentlySentHeartbeatLocalSendTimes;  // hbPacket ID -> local-send-time

   Hashtable<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> _onlineSources;
   Hashtable<ZGPeerID, Queue<IPAddressAndPort> > _peerIDToIPAddresses;

   bool _updateOfficialPeersListPending;
   bool _forceOfficialPeersUpdate;

   Hashtable<PZGHeartbeatSourceKey, Void> _lastSourcesSentToMaster;

   uint16 _heartbeatSourceTagCounter;  // used to give a succinct-yet-unique ID to each heartbeat-destination we send out
   Queue<PacketDataIORef> _mdioKeys;   // used to detect when the DataIOs have changed
   Hashtable<uint16, IPAddressAndPort> _heartbeatSourceTagToDest;
   Hashtable<IPAddressAndPort, uint16> _heartbeatSourceDestToTag;
   ZLibCodec _zlibCodec;

   Mutex _mainThreadLatenciesLock;
   Hashtable<ZGPeerID, uint64> _mainThreadLatencies;

   Hashtable<ZGPeerID, uint64> _lastMismatchedVersionLogTimes;
};

};  // end namespace zg_private

#include "dataio/UDPSocketDataIO.h"
#include "dataio/SimulatedMulticastDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"

#include "zg/ZGConstants.h"
#include "zg/private/PZGConstants.h"
#include "zg/private/PZGHeartbeatPacket.h"
#include "zg/private/PZGHeartbeatSession.h"
#include "zg/private/PZGNetworkIOSession.h"
#include "zg/private/PZGHeartbeatSourceState.h"

namespace zg_private
{


enum {
   PZG_HEARTBEAT_COMMAND_PEERS_UPDATE = 1751281781 // 'hbpu' 
};

static const String PZG_HEARTBEAT_NAME_PEERINFO = "hpi";
static const String PZG_HEARTBEAT_NAME_PEER_ID  = "pid";

static PZGHeartbeatPacketWithMetaDataRef GetHeartbeatPacketWithMetaDataFromPool()
{
   static PZGHeartbeatPacketWithMetaDataRef::ItemPool _heartbeatPool;
   return PZGHeartbeatPacketWithMetaDataRef(_heartbeatPool.ObtainObject());
}

PZGHeartbeatThreadState :: PZGHeartbeatThreadState() : _zlibCodec(9)
{
   // empty 
}

PZGHeartbeatThreadState :: ~PZGHeartbeatThreadState()
{
   // empty 
}

void PZGHeartbeatThreadState :: Initialize(const ConstPZGHeartbeatSettingsRef & hbSettings, uint64 startTime) 
{
   _hbSettings                        = hbSettings;
   _heartbeatThreadStateBirthdate     = startTime;
   _heartbeatPingInterval             = SecondsToMicros(1)/muscleMax((uint32)1, _hbSettings()->GetHeartbeatsPerSecond());
   _heartbeatExpirationTimeMicros     = (_heartbeatPingInterval*_hbSettings()->GetMaxNumMissingHeartbeats());
   _nextSendHeartbeatTime             = startTime;
   _now                               = startTime;
   _halfAttachedTime                  = startTime+((_hbSettings()->GetHeartbeatsBeforeFullyAttached()*_heartbeatPingInterval)/2);
   _fullyAttachedTime                 = startTime+((_hbSettings()->GetHeartbeatsBeforeFullyAttached()*_heartbeatPingInterval)/1);
   _fullAttachmentReported            = false;
   _toNetworkTimeOffset               = INVALID_TIME_OFFSET;
   _mainThreadToNetworkTimeOffset     = INVALID_TIME_OFFSET;
   _updateToNetworkTimeOffsetPending  = false;
   _recreateMulticastDataIOsRequested = true;
   _updateOfficialPeersListPending    = false;
   _forceOfficialPeersUpdate          = false;
   _heartbeatSourceTagCounter         = 0;
   _mdioKeys.Clear();
}

uint64 PZGHeartbeatThreadState :: GetPulseTime() const
{
   if ((_updateOfficialPeersListPending)||(_updateToNetworkTimeOffsetPending)) return 0;

   uint64 ret = _nextSendHeartbeatTime;
   for (HashtableIterator<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> iter(_onlineSources); iter.HasData(); iter++) ret = muscleMin(ret, iter.GetValue()()->GetLocalExpirationTimeMicros());
   return ret;
}

void PZGHeartbeatThreadState :: EnsureHeartbeatSourceTagsTableUpdated()
{
   if (_multicastDataIOs == _mdioKeys) return;  // no point in regenerating again from the same UDPSocketDataIO object!
   _mdioKeys = _multicastDataIOs;

   // Remove any no-longer-present destinations from our lookup tables
   Queue<IPAddressAndPort> dests;
   for (uint32 i=0; i<_multicastDataIOs.GetNumItems(); i++)
   {
      const IPAddressAndPort & iap = _multicastDataIOs[i]()->GetPacketSendDestination();
      if (iap.IsValid()) (void) dests.AddTail(iap);  // semi-paranoia; they should always be valid
   }
   
   for (HashtableIterator<IPAddressAndPort, uint16> iter(_heartbeatSourceDestToTag); iter.HasData(); iter++)
   {
      const IPAddressAndPort & iap = iter.GetKey();
      if (dests.Contains(iap) == false)
      {
         // Let's get rid of any time-averagers that were keyed to this IP address, since they won't get used anymore
         for (HashtableIterator<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> iter(_onlineSources); iter.HasData(); iter++) (void) iter.GetValue()()->DiscardRoundTripTimeAverager(iap);

         uint16 tag = 0;
         if (_heartbeatSourceDestToTag.Remove(iap, tag).IsOK()) (void) _heartbeatSourceTagToDest.Remove(tag);
      }
   }

   if ((_multicastDataIOs.HasItems())&&(_heartbeatSourceTagToDest.EnsureCanPut(dests.GetNumItems()).IsOK())&&(_heartbeatSourceDestToTag.EnsureCanPut(dests.GetNumItems()).IsOK()))
   {
      // Then add any newly-present destinations to our lookup tables
      for (uint32 i=0; i<dests.GetNumItems(); i++)
      {
         const IPAddressAndPort & iap = dests[i];
         if (_heartbeatSourceDestToTag.ContainsKey(iap) == false)
         {
            if (++_heartbeatSourceTagCounter == 0) _heartbeatSourceTagCounter = 1;  // semi-paranoia (I want 0 to always be a guard/NULL value)

            const uint16 nextTag = _heartbeatSourceTagCounter;
            (void) _heartbeatSourceTagToDest.Put(nextTag, iap);
            (void) _heartbeatSourceDestToTag.Put(iap, nextTag);
         }
      }
   }
}

static const uint32 HB_HEADER_SIZE  = sizeof(uint16) + sizeof(uint16) + sizeof(uint64) + sizeof(uint32);  // HB_HEADER_MAGIC, heartbeatSourceTag, networkSendTimeMicros, payload checksum
static const uint16 HB_HEADER_MAGIC = 25874;  // a completely arbitrary 16-bit value
static bool _printTimeSynchronizationDeltas = false;
void SetEnableTimeSynchronizationDebugging(bool e) {_printTimeSynchronizationDeltas = e;}

void PZGHeartbeatThreadState :: Pulse(Queue<MessageRef> & messagesForOwnerThread)
{
   if (_updateOfficialPeersListPending)
   {
      bool force = _forceOfficialPeersUpdate;
      _updateOfficialPeersListPending = _forceOfficialPeersUpdate = false;

      MessageRef msg = UpdateOfficialPeersList(force);
      if (msg()) (void) messagesForOwnerThread.AddTail(msg);
   }

   if (_updateToNetworkTimeOffsetPending) UpdateToNetworkTimeOffset();

   if (_now >= _nextSendHeartbeatTime)
   {
      _nextSendHeartbeatTime = _now+_heartbeatPingInterval;
      if (SendHeartbeatPackets().IsError()) LogTime(MUSCLE_LOG_ERROR, "SendHeartbeatPackets() failed!\n");
      if (_printTimeSynchronizationDeltas) PrintTimeSynchronizationDeltas();

      // Update the main-thread-accessible latencies table, just so we don't have to lock our own data structures all the time
      DECLARE_MUTEXGUARD(_mainThreadLatenciesLock);
      for (HashtableIterator<ZGPeerID, Queue<IPAddressAndPort> > iter(_peerIDToIPAddresses); iter.HasData(); iter++)
      {
         const ZGPeerID & peerID = iter.GetKey();
         const Queue<IPAddressAndPort> & sourceQ = iter.GetValue();
         PZGHeartbeatSourceState * hss = sourceQ.HasItems() ? _onlineSources[PZGHeartbeatSourceKey(sourceQ.Head(), peerID)]() : NULL;
         const PZGHeartbeatPacketWithMetaData * peerHB = hss ? hss->GetHeartbeatPacket()() : NULL;
         (void) _mainThreadLatencies.Put(peerID, peerHB ? hss->GetPreferredAverageValue(0) : MUSCLE_TIME_NEVER);
      }
   }

   for (HashtableIterator<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> iter(_onlineSources); iter.HasData(); iter++)
      if (_now >= iter.GetValue()()->GetLocalExpirationTimeMicros()) ExpireSource(iter.GetKey());

   if ((_fullAttachmentReported == false)&&(IsFullyAttached()))
   {
      LogTime(MUSCLE_LOG_DEBUG, "Fully attached!\n");
      _fullAttachmentReported = true;
      ScheduleUpdateOfficialPeersList(true);  // now that we're fully attached we'll tell our master what's up
   }
}

status_t PZGHeartbeatThreadState :: SendHeartbeatPackets()
{
   if (_multicastDataIOs.IsEmpty()) return B_ERROR;  // nothing to send to?

   PZGHeartbeatPacketWithMetaDataRef hbRef = GetHeartbeatPacketWithMetaDataFromPool();
   MRETURN_OOM_ON_NULL(hbRef());

   if (hbRef()) hbRef()->Initialize(*_hbSettings(), (uint32) MicrosToSeconds(_now-_heartbeatThreadStateBirthdate), IsFullyAttached(), ++_hbSettings()->_outgoingHeartbeatPacketIDCounter);

   PZGHeartbeatPacketWithMetaData & hb = *hbRef();
   if ((_hbSettings()->GetPeerType() == PEER_TYPE_FULL_PEER)&&(_now >= _halfAttachedTime)) 
   {
      const Queue<ZGPeerID> pids = CalculateOrderedPeersList();
      Queue<ConstPZGHeartbeatPeerInfoRef> & hpis = hb.GetOrderedPeersList();
      (void) hpis.EnsureSize(pids.GetNumItems());
      for (uint32 i=0; i<pids.GetNumItems(); i++) 
      {
         ConstPZGHeartbeatPeerInfoRef hpiRef = GetPZGHeartbeatPeerInfoRefFor(_now, pids[i]);
         if (hpiRef()) hpis.AddTail(hpiRef);
                  else LogTime(MUSCLE_LOG_ERROR, "GetPZGHeartbeatPeerInfoRefFor() returned a NULL reference for peer [%s]\n", pids[i].ToString()());
      }
   }

   MRETURN_ON_ERROR(_rawScratchBuf.SetNumBytes(hb.FlattenedSize(), false));
   hb.Flatten(_rawScratchBuf.GetBuffer());

   // Zlib-compress the heartbeat packet data into _deflatedScratchBuf, to keep our heartbeat-packet sizes down
   status_t ret;
   if (_zlibCodec.Deflate(_rawScratchBuf, true, _deflatedScratchBuf, HB_HEADER_SIZE).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't deflate outgoing heartbeat data!\n");
      return ret;
   }

   // If the UDPSocketDataIO was replaced, then we need to generate new tag-IDs for the new one
   EnsureHeartbeatSourceTagsTableUpdated();

   // Write out the static header bytes
   const uint32 defBufSize = _deflatedScratchBuf.GetNumBytes();
   uint8 * dsb = _deflatedScratchBuf.GetBuffer();
   muscleCopyOut(dsb, B_HOST_TO_LENDIAN_INT16(HB_HEADER_MAGIC));     // the first two bytes are magic bytes, used for quick bogus-packet filtering
   muscleCopyOut(dsb+(2*sizeof(uint16))+sizeof(uint64), B_HOST_TO_LENDIAN_INT32(CalculateChecksum(dsb+HB_HEADER_SIZE, defBufSize-HB_HEADER_SIZE)));  // so the receiver can check if the zlib data got corrupted somehow

   for (uint32 i=0; i<_multicastDataIOs.GetNumItems(); i++)
   {
      PacketDataIO * dio = _multicastDataIOs[i]();
      const IPAddressAndPort & dest = dio->GetPacketSendDestination();
      const uint16 * tag = _heartbeatSourceDestToTag.Get(dest);
      if (tag)
      {
         // Write out the dynamic (per-interface) header bytes
         muscleCopyOut(dsb+(1*sizeof(uint16)), B_HOST_TO_LENDIAN_INT16(*tag)); // so when we get heartbeats back from a peer later we know which of our interfaces the included timing info corresponds to
         muscleCopyOut(dsb+(2*sizeof(uint16)), GetNetworkTime64ForRunTime64(GetRunTime64())); // network-clock-at-send-time

         const int32 numBytesSent = dio->Write(dsb, defBufSize);
         if (numBytesSent != (int32)defBufSize) LogTime(MUSCLE_LOG_ERROR, "Error sending heartbeat to [%s], sent " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes!\n", dest.ToString()(), numBytesSent, defBufSize);
      }
   }

   // Store some recently-sent heartbeats so that we can consult them later on, to compute packet-round-trip times
   if ((_recentlySentHeartbeatLocalSendTimes.Put(hb.GetHeartbeatPacketID(), _now).IsOK())&&(_recentlySentHeartbeatLocalSendTimes.GetNumItems() > 100)) (void) _recentlySentHeartbeatLocalSendTimes.RemoveFirst();

   return B_NO_ERROR;
} 


void PZGHeartbeatThreadState :: PrintTimeSynchronizationDeltas() const
{
   printf("\n\n======== CHECK TIMES =========\n");
   for (HashtableIterator<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> iter(_onlineSources); iter.HasData(); iter++)
   {
      const PZGHeartbeatSourceState & sourceData = *iter.GetValue()();
      const PZGHeartbeatPacketWithMetaData * hb = sourceData.GetHeartbeatPacket()();
      if ((hb)&&(hb->IsFullyAttached())) printf(" [%s] -> %c[%s]\n", iter.GetKey().ToString()(), (hb->GetSourcePeerID()==_hbSettings()->GetLocalPeerID())?'*':' ', iter.GetValue()()->ToString(*this)());
   }
}

void PZGHeartbeatThreadState :: UpdateToNetworkTimeOffset()
{
   _updateToNetworkTimeOffsetPending = false;
   if (IsAtLeastHalfAttached())
   {
      if (IAmTheSeniorPeer()) _mainThreadToNetworkTimeOffset = _toNetworkTimeOffset = 0; // senior peer is always exactly synced with itself, by definition
      else
      {
         const ZGPeerID & seniorPID = GetSeniorPeerID();
         const Queue<IPAddressAndPort> * sourceQ = _peerIDToIPAddresses.Get(seniorPID);
         PZGHeartbeatSourceState * hss = ((sourceQ)&&(sourceQ->HasItems())) ? _onlineSources[PZGHeartbeatSourceKey(sourceQ->Head(), seniorPID)]() : NULL;
         const PZGHeartbeatPacketWithMetaData * seniorHB = hss ? hss->GetHeartbeatPacket()() : NULL;
         if (seniorHB) 
         {
            const uint64 roundTripTimeMicros = hss->GetPreferredAverageValue(_now-_heartbeatExpirationTimeMicros);
            const uint64 seniorNetTime = seniorHB->GetNetworkSendTimeMicros();
            const uint64 localRecvTime = seniorHB->GetLocalReceiveTimeMicros();
            _mainThreadToNetworkTimeOffset = _toNetworkTimeOffset = seniorNetTime-(localRecvTime-(roundTripTimeMicros/2));
//printf("UpdateNetworkTimeOffset seniorPeer=[%s] source=[%s]:  seniorNetTime was " UINT64_FORMAT_SPEC " localTimeIReceivedThatAt was " UINT64_FORMAT_SPEC " rttAvg=" UINT64_FORMAT_SPEC " estRoundTripTime=" UINT64_FORMAT_SPEC " --> offset is " INT64_FORMAT_SPEC "\n", GetSeniorPeerID().ToString()(), seniorHB->GetPacketSource().ToString()(), seniorNetTime, localRecvTime, hss->GetPreferredAverageValue(0), roundTripTimeMicros, _toNetworkTimeOffset);
         }
      }
   }
}

// Returns true iff the ZGPeerIDs in (idQ) are the same as the keys in our own _peerIDToIPAddresses list (ordering doesn't matter)
bool PZGHeartbeatThreadState :: PeersListMatchesIgnoreOrdering(const Queue<ConstPZGHeartbeatPeerInfoRef> & infoQ) const
{
   if (infoQ.GetNumItems() != _peerIDToIPAddresses.GetNumItems()) return false;
   for (uint32 i=0; i<infoQ.GetNumItems(); i++) if (_peerIDToIPAddresses.ContainsKey(infoQ[i]()->GetPeerID()) == false) return false;
   return true;
}

// Returns the peer with the lowest peer ID that also has the same advertised peer-IDs-list that we do (not counting peer-ordering)
ZGPeerID PZGHeartbeatThreadState :: GetKingmakerPeerID() const
{
   ZGPeerID minPeerID;
   for (HashtableIterator<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> iter(_onlineSources); iter.HasData(); iter++)
   {
      const PZGHeartbeatPacketWithMetaData & hbPacket = *iter.GetValue()()->GetHeartbeatPacket()();
      const ZGPeerID & nextPID = hbPacket.GetSourcePeerID();
      if (((minPeerID.IsValid() == false)||(nextPID < minPeerID))&&(PeersListMatchesIgnoreOrdering(hbPacket.GetOrderedPeersList()))) minPeerID = nextPID;
   }
   return minPeerID;
}

// Returns the source with the lowest peer ID that also has the same advertised peer-IDs-list that we do (not counting peer-ordering)
PZGHeartbeatSourceKey PZGHeartbeatThreadState :: GetKingmakerPeerSource() const
{
   PZGHeartbeatSourceKey ret;

   ZGPeerID minPeerID;
   for (HashtableIterator<PZGHeartbeatSourceKey, PZGHeartbeatSourceStateRef> iter(_onlineSources); iter.HasData(); iter++)
   {
      const PZGHeartbeatPacketWithMetaData & hbPacket = *iter.GetValue()()->GetHeartbeatPacket()();
      const ZGPeerID & nextPID = hbPacket.GetSourcePeerID();
      if (((minPeerID.IsValid() == false)||(nextPID < minPeerID))&&(PeersListMatchesIgnoreOrdering(hbPacket.GetOrderedPeersList()))) 
      {
         ret       = iter.GetKey();
         minPeerID = nextPID;
      }
   }
   return ret;
}

class ComparePeerIDsBySeniorityFunctor
{
public:
   int Compare(const ZGPeerID & pid1, const ZGPeerID & pid2, void * cookie) const {return ((const PZGHeartbeatThreadState *)cookie)->ComparePeerIDsBySeniority(pid1, pid2);}
};

uint16 PZGHeartbeatThreadState :: GetPeerTypeFromQueue(const ZGPeerID & pid, const Queue<IPAddressAndPort> & q) const
{
   uint16 ret = 0;
   for (uint32 i=0; i<q.GetNumItems(); i++)
   {
      const PZGHeartbeatSourceStateRef * r = _onlineSources.Get(PZGHeartbeatSourceKey(q[i], pid));
      const PZGHeartbeatPacketWithMetaData * hb = r ? r->GetItemPointer()->GetHeartbeatPacket()() : NULL;
      if (hb) ret = muscleMax(ret, hb->GetPeerType());  // semi-paranoia since they should all be the same anyway
         else LogTime(MUSCLE_LOG_ERROR, "GetPeerTypeFromQueue:  Couldn't find record for source %s\n", q[i].ToString()());
   }
   return ret;
}

uint32 PZGHeartbeatThreadState :: GetPeerUptimeSecondsFromQueue(const ZGPeerID & pid, const Queue<IPAddressAndPort> & q) const
{
   uint32 ret = 0;
   for (uint32 i=0; i<q.GetNumItems(); i++)
   {
      const PZGHeartbeatSourceStateRef * r = _onlineSources.Get(PZGHeartbeatSourceKey(q[i], pid));
      const PZGHeartbeatPacketWithMetaData * hb = r ? r->GetItemPointer()->GetHeartbeatPacket()() : NULL;
      if (hb) ret = muscleMax(ret, hb->GetPeerUptimeSeconds());
         else LogTime(MUSCLE_LOG_ERROR, "GetPeerUptimeSecondsFromQueue:  Couldn't find record for source %s\n", q[i].ToString()());
   }
   return ret;
}

int PZGHeartbeatThreadState :: ComparePeerIDsBySeniority(const ZGPeerID & pid1, const ZGPeerID & pid2) const
{
   const Queue<IPAddressAndPort> * q1 = _peerIDToIPAddresses.Get(pid1);
   const Queue<IPAddressAndPort> * q2 = _peerIDToIPAddresses.Get(pid2);
   if ((q1)&&(q2))
   {
      // so that junior-only peers always stay towards the end of the list
      const uint16 pt1 = GetPeerTypeFromQueue(pid1, *q1);
      const uint16 pt2 = GetPeerTypeFromQueue(pid2, *q2);
      int ret = -muscleCompare(pt1, pt2);
      if (ret) return ret;

      // Mostly we sort by uptime though, so that any newbies or peers who crashed and restarted will be seen as more junior
      const uint32 ut1 = GetPeerUptimeSecondsFromQueue(pid1, *q1);
      const uint32 ut2 = GetPeerUptimeSecondsFromQueue(pid2, *q2);
      ret = -muscleCompare(ut1, ut2);
      if (ret) return ret;

      // But if all else fails we'll sort by peer ID, just to keep the sort as stable as we can
      return -muscleCompare(pid1, pid2);
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "ComparePeerIDsBySeniority:  Queue doesn't exist?  " UINT32_FORMAT_SPEC ", " UINT32_FORMAT_SPEC "\n", q1?q1->GetNumItems():666, q2?q2->GetNumItems():666);
      return 0;
   }
};

Queue<ZGPeerID> PZGHeartbeatThreadState :: CalculateOrderedPeersList()
{
   Queue<ZGPeerID> ret;

   const PZGHeartbeatSourceKey kmSource = GetKingmakerPeerSource();
   if (kmSource.IsValid())
   {
      // If we know who the kingmaker peer is, we'll just adopt the peer-ordering he is advertising, for uniformity's sake
      // Note that if we got here, we are guaranteed that kmPeer's IDs-list has the same entries as our _peerIDToIPAddresses list (albeit maybe not in the same order)
      const PZGHeartbeatSourceStateRef * kmSourceData = _onlineSources.Get(kmSource);
      if (kmSourceData)
      {
         const Queue<ConstPZGHeartbeatPeerInfoRef> & kmq = kmSourceData->GetItemPointer()->GetHeartbeatPacket()()->GetOrderedPeersList();
         if (ret.EnsureSize(kmq.GetNumItems()).IsOK()) for (uint32 i=0; i<kmq.GetNumItems(); i++) (void) ret.AddTail(kmq[i]()->GetPeerID());
      }
   }
   else
   {
      // If we don't know who the kingmaker peer is, then we'll populate the list based solely on our own local sorting-criteria.
      _peerIDToIPAddresses.SortByKey(ComparePeerIDsBySeniorityFunctor(), this);
      if (ret.EnsureSize(_peerIDToIPAddresses.GetNumItems()).IsOK()) for (HashtableIterator<ZGPeerID, Queue<IPAddressAndPort> > iter(_peerIDToIPAddresses); iter.HasData(); iter++) (void) ret.AddTail(iter.GetKey());
   }

   return ret;
}

ConstPZGHeartbeatPeerInfoRef PZGHeartbeatThreadState :: GetPZGHeartbeatPeerInfoRefFor(uint64 now, const ZGPeerID & peerID) const
{
   PZGHeartbeatPeerInfoRef ret = GetPZGHeartbeatPeerInfoFromPool();
   if (ret() == NULL) return ConstPZGHeartbeatPeerInfoRef();  // doh!

   ret()->SetPeerID(peerID);

   const Queue<IPAddressAndPort> * sources = _peerIDToIPAddresses.Get(peerID);
   if ((sources)&&(sources->HasItems()))
   {
      for (uint32 i=0; i<sources->GetNumItems(); i++)
      {
         const IPAddressAndPort & source = (*sources)[i];
         const PZGHeartbeatSourceStateRef * peerRecord = _onlineSources.Get(PZGHeartbeatSourceKey(source, peerID));
         if (peerRecord)
         {
            PZGHeartbeatPacketWithMetaData & phb = *peerRecord->GetItemPointer()->GetHeartbeatPacket()();
            if (phb.HaveSentTimingReply() == false)
            {
               phb.SetHaveSentTimingReply(true);
               ret()->PutTimingInfo(phb.GetSourceTag(), phb.GetHeartbeatPacketID(), (uint32) muscleMin(now-phb.GetLocalReceiveTimeMicros(), (uint64)((uint32)-1)));
            }
         } 
      }
   }

   return AddConstToRef(ret);
}

void PZGHeartbeatThreadState :: MessageReceivedFromOwner(const MessageRef & msgFromOwner)
{
   switch(msgFromOwner()->what)
   {
      case PZG_THREADED_SESSION_RECREATE_SOCKETS:
         LogTime(MUSCLE_LOG_DEBUG, "Heartbeat multicast thread:  forcing recreation of sockets, in response to network configuration change\n");
         _recreateMulticastDataIOsRequested = true;
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "Heartbeat thread received unknown Message " UINT32_FORMAT_SPEC "\n", msgFromOwner()->what);
      break;
   }
}

PZGHeartbeatPacketWithMetaDataRef PZGHeartbeatThreadState :: ParseHeartbeatPacketBuffer(const ByteBuffer & defBuf, const IPAddressAndPort & sourceIAP, uint64 localReceiveTimeMicros)
{
   const uint32 numBytes = defBuf.GetNumBytes();
   if (numBytes < HB_HEADER_SIZE)
   {
      LogTime(MUSCLE_LOG_ERROR, "ParseHeartbeatPacketBuffer from [%s]:  buffer is too short!  (" UINT32_FORMAT_SPEC " bytes:  %s)\n", sourceIAP.ToString()(), numBytes, HexBytesToString(defBuf)());
      return PZGHeartbeatPacketWithMetaDataRef();
   }

   const uint8 * dsb = defBuf.GetBuffer();
   const uint16 hbMagic = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(dsb));
   if (hbMagic != HB_HEADER_MAGIC)
   {
      LogTime(MUSCLE_LOG_ERROR, "ParseHeartbeatPacketBuffer from [%s]:  bad header magic:  expected %u, got %u\n", sourceIAP.ToString()(), HB_HEADER_MAGIC, hbMagic);
      return PZGHeartbeatPacketWithMetaDataRef();
   }

   const uint32 hisChecksum = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(dsb+(2*sizeof(uint16))+sizeof(uint64)));
   const uint32 myChecksum  = CalculateChecksum(dsb+HB_HEADER_SIZE, numBytes-HB_HEADER_SIZE);
   if (hisChecksum != myChecksum) 
   {
      LogTime(MUSCLE_LOG_ERROR, "ParseHeartbeatPacketBuffer from [%s]:  Bad checksum on " UINT32_FORMAT_SPEC "-byte heartbeat packet; expected " UINT32_FORMAT_SPEC", got " UINT32_FORMAT_SPEC ".\n", sourceIAP.ToString()(), numBytes, myChecksum, hisChecksum);
      return PZGHeartbeatPacketWithMetaDataRef();
   }

   PZGHeartbeatPacketWithMetaDataRef newHB = GetHeartbeatPacketWithMetaDataFromPool();
   if (newHB() == NULL)
   {
      MWARN_OUT_OF_MEMORY;
      return PZGHeartbeatPacketWithMetaDataRef();
   }

   if (_zlibCodec.Inflate(defBuf.GetBuffer()+HB_HEADER_SIZE, defBuf.GetNumBytes()-HB_HEADER_SIZE, _rawScratchBuf).IsError())
   {
      LogTime(MUSCLE_LOG_ERROR, "ParseHeartbeatPacketBuffer from [%s]:  Couldn't inflate " UINT32_FORMAT_SPEC " bytes of compressed PZGHeartbeatPacket data!\n", sourceIAP.ToString()(), defBuf.GetNumBytes()-HB_HEADER_SIZE);
      return PZGHeartbeatPacketWithMetaDataRef();
   }

   if (newHB()->Unflatten(_rawScratchBuf.GetBuffer(), _rawScratchBuf.GetNumBytes()).IsError())
   {
      LogTime(MUSCLE_LOG_ERROR, "ParseHeartbeatPacketBuffer from [%s]:  Couldn't unflatten PZGHeartbeatPacket from " UINT32_FORMAT_SPEC " bytes of uncompressed data!\n", sourceIAP.ToString()(), _rawScratchBuf.GetNumBytes());
      return PZGHeartbeatPacketWithMetaDataRef();
   }

   newHB()->SetNetworkSendTimeMicros(B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(dsb+sizeof(uint16)+sizeof(uint16))));  // sent outside of the zlib-compression, for better timestamp-accuracy
   newHB()->SetPacketSource(sourceIAP, B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(dsb+sizeof(uint16))));
   newHB()->SetLocalReceiveTimeMicros(localReceiveTimeMicros);
   return newHB;
}

void PZGHeartbeatThreadState :: ReceiveMulticastTraffic(PacketDataIO & dio)
{
   while(_deflatedScratchBuf.SetNumBytes(2048, false).IsOK())  // we want to start each read with the full space available
   {
      int32 numBytesRead;
      if ((numBytesRead = dio.Read(_deflatedScratchBuf.GetBuffer(), _deflatedScratchBuf.GetNumBytes())) != 0)
      {
         const uint64 localReceiveTimeMicros = GetRunTime64();

         if (numBytesRead < 0)
         {
            LogTime(MUSCLE_LOG_ERROR, "ReceiveMulticastTraffic:  Error reading from multicast socket! (read " INT32_FORMAT_SPEC " bytes)\n", numBytesRead);
            break;
         }

         (void) _deflatedScratchBuf.SetNumBytes(numBytesRead, true);  // we only care about valid bytes now

         const IPAddressAndPort & sourceIAP = dio.GetSourceOfLastReadPacket();
         PZGHeartbeatPacketWithMetaDataRef newHB = ParseHeartbeatPacketBuffer(_deflatedScratchBuf, sourceIAP, localReceiveTimeMicros);
         if (newHB())
         {
            const ZGPeerID & pid = newHB()->GetSourcePeerID();
            if (newHB()->GetVersionCode() != _hbSettings()->GetVersionCode())
            {
               // we'll ignore heartbeat packets from ZG peers that are marked with a different version, since we don't know how they behave and therefore we'd rather not communicate with them at all.
               uint64 * lastTime = _lastMismatchedVersionLogTimes.GetOrPut(pid);
               if ((lastTime)&&(OnceEvery(SecondsToMicros(1), *lastTime))) LogTime(MUSCLE_LOG_ERROR, "Received heartbeat-packet from peer [%s] with compatibility-version [%s], but this application requires compatibility-version [%s]:  Ignoring the packet\n", pid.ToString()(), CompatibilityVersionCodeToString(newHB()->GetVersionCode())(), CompatibilityVersionCodeToString(_hbSettings()->GetCompatibilityVersionCode())());
               continue;
            }

            PZGHeartbeatSourceKey source(sourceIAP, pid);

            if (newHB()->GetSystemKey() == _hbSettings()->GetSystemKey())
            {
               // See if we can use this heartbeat to compute an estimate of the multicast-packet-round-trip time (from us to him to us)
               const Queue<ConstPZGHeartbeatPeerInfoRef> & opq = newHB()->GetOrderedPeersList();
               for (uint32 i=0; i<opq.GetNumItems(); i++)
               {
                  const PZGHeartbeatPeerInfo & pi = *opq[i]();
                  if (pi.GetPeerID() == _hbSettings()->GetLocalPeerID())
                  {
                     const Queue<PZGHeartbeatPeerInfo::PZGTimingInfo> & tis = pi.GetTimingInfos();
                     for (uint32 j=0; j<tis.GetNumItems(); j++)
                     {
                        const PZGHeartbeatPeerInfo::PZGTimingInfo & ti = tis[j];
                        const IPAddressAndPort * multicastIAP = _heartbeatSourceTagToDest.Get(ti.GetSourceTag());  // an ff12::blah multicast address
                        if (multicastIAP)
                        {
                           const uint32 dwellTime = ti.GetDwellTimeMicros();
                           const uint64 * packetLocalSendTime = (dwellTime == MUSCLE_NO_LIMIT) ? NULL : _recentlySentHeartbeatLocalSendTimes.Get(ti.GetSourceHeartbeatPacketID());
                           PZGHeartbeatSourceStateRef * sourceInfo = packetLocalSendTime ? _onlineSources.Get(source) : NULL;
                           if (sourceInfo) (void) sourceInfo->GetItemPointer()->AddMeasurement(*multicastIAP, localReceiveTimeMicros-(*packetLocalSendTime+dwellTime), _now);
                           break;
                        }
                     }
                     break;
                  }
               }

               PZGHeartbeatSourceStateRef oldSource = _onlineSources[source];
               ConstPZGHeartbeatPacketWithMetaDataRef oldHB; if (oldSource()) oldHB = oldSource()->GetHeartbeatPacket();

               const uint64 localExpirationTimeMicros = (pid==_hbSettings()->GetLocalPeerID())?MUSCLE_TIME_NEVER:(localReceiveTimeMicros+_heartbeatExpirationTimeMicros);
               if ((oldHB())&&(newHB()->IsEqualIgnoreTransients(*oldHB())))
               {
                  if ((pid != _hbSettings()->GetLocalPeerID())&&(GetMaxLogLevel() >= MUSCLE_LOG_TRACE)) LogTime(MUSCLE_LOG_TRACE, "Source %s:  heartbeat interval was [%s]\n", source.ToString()(), GetHumanReadableSignedTimeIntervalString(localReceiveTimeMicros-oldHB()->GetLocalReceiveTimeMicros(), 1)());

                  // When a peer becomes fully attached we'll force a resend because we don't tell the main thread about non-fully-attached peers
                  if (oldHB()->IsFullyAttached() != newHB()->IsFullyAttached()) ScheduleUpdateOfficialPeersList(true);

                  oldSource()->SetHeartbeatPacket(newHB, localExpirationTimeMicros);
               }
               else
               {
                  if (oldSource()) ExpireSource(source);  // out with the old version (if any)
                  IntroduceSource(source, newHB, localExpirationTimeMicros); // and in with the new
               }

               if ((_updateOfficialPeersListPending == false)&&(pid == GetKingmakerPeerID())) ScheduleUpdateOfficialPeersList(false);
               if (pid == GetSeniorPeerID()) ScheduleUpdateToNetworkTimeOffset();
            }
            else LogTime(MUSCLE_LOG_WARNING, "Incoming HeartbeatPacket from [%s] had wrong systemKey hash for system [%s / %s] (" UINT64_FORMAT_SPEC ", expected " UINT64_FORMAT_SPEC ")\n", source.ToString()(), _hbSettings()->GetSignature()(), _hbSettings()->GetSystemName()(), newHB()->GetSystemKey(), _hbSettings()->GetSystemKey());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error, couldn't parse incoming heartbeat packet from [%s]\n", sourceIAP.ToString()());
      }
      else break;  // nothing more to read!
   }
}

MessageRef PZGHeartbeatThreadState :: UpdateOfficialPeersList(bool forceUpdate)
{
   MessageRef ret;

   const Queue<ZGPeerID> idQ = CalculateOrderedPeersList();

   // Convert the list of ZGPeerIDs into the equivalent list of ConstPZGHeartbeatPacketWithMetaDataRef's
   // note that a given ZGPeerID may have more than one ConstPZGHeartbeatPacketWithMetaDataRef, if we
   // are receiving heartbeats from it on more than one network interface.
   Hashtable<PZGHeartbeatSourceKey, Void> newPeers; (void) newPeers.EnsureSize(_onlineSources.GetNumItems());
   for (uint32 i=0; i<idQ.GetNumItems(); i++)
   {
      const ZGPeerID & pid = idQ[i];
      const Queue<IPAddressAndPort> * q = _peerIDToIPAddresses.Get(pid);
      if (q) for (uint32 j=0; j<q->GetNumItems(); j++) (void) newPeers.PutWithDefault(PZGHeartbeatSourceKey((*q)[j], pid));
   }

   if ((forceUpdate)||(newPeers != _lastSourcesSentToMaster))
   {
      _lastSourcesSentToMaster = newPeers;
      if (IsFullyAttached())
      {
         ret = GetMessageFromPool(PZG_HEARTBEAT_COMMAND_PEERS_UPDATE);
         if (ret())
         {
            for (HashtableIterator<PZGHeartbeatSourceKey, Void> iter(_lastSourcesSentToMaster); iter.HasData(); iter++)
            {
               const PZGHeartbeatSourceStateRef * sourceInfo = _onlineSources.Get(iter.GetKey());
               PZGHeartbeatPacketWithMetaDataRef hbRef; if (sourceInfo) hbRef = sourceInfo->GetItemPointer()->GetHeartbeatPacket();
               if ((hbRef())&&(hbRef()->IsFullyAttached())) (void) ret()->AddFlat(PZG_HEARTBEAT_NAME_PEERINFO, FlatCountableRef(hbRef, false));
            }
         }
      }
   }
   UpdateToNetworkTimeOffset();  // in case the senior peer has changed
   return ret;
}

void PZGHeartbeatThreadState :: ExpireSource(const PZGHeartbeatSourceKey & source)
{
   PZGHeartbeatSourceStateRef sourceInfo = _onlineSources[source];
   if (sourceInfo())
   {
      LogTime(MUSCLE_LOG_DEBUG, "Source [%s] is now offline [%s].\n", source.ToString()(), sourceInfo()->GetHeartbeatPacket()()->ToString()());

      const ZGPeerID & pid = sourceInfo()->GetHeartbeatPacket()()->GetSourcePeerID();
      Queue<IPAddressAndPort> * q = _peerIDToIPAddresses.Get(pid);
      if ((q)&&(q->RemoveFirstInstanceOf(source.GetIPAddressAndPort()).IsOK())&&(q->IsEmpty())) 
      {
         (void) _peerIDToIPAddresses.Remove(pid);

         DECLARE_MUTEXGUARD(_mainThreadLatenciesLock);
         _mainThreadLatencies.Remove(pid);
      }

      (void) _onlineSources.Remove(source);
      ScheduleUpdateOfficialPeersList(true);
   }
}

void PZGHeartbeatThreadState :: IntroduceSource(const PZGHeartbeatSourceKey & source, const PZGHeartbeatPacketWithMetaDataRef & newHB, uint64 localExpirationTimeMicros)
{
   PZGHeartbeatSourceStateRef newSource(newnothrow PZGHeartbeatSourceState(20));
   if (newSource() == NULL) {MWARN_OUT_OF_MEMORY; return;}

   newSource()->SetHeartbeatPacket(newHB, localExpirationTimeMicros);
   if (_onlineSources.Put(source, newSource).IsOK())
   {
      const ZGPeerID & pid = newHB()->GetSourcePeerID();

      Queue<IPAddressAndPort> * q = _peerIDToIPAddresses.GetOrPut(pid);
      if (q) (void) q->AddTail(source.GetIPAddressAndPort());

      ScheduleUpdateOfficialPeersList(true);
      LogTime(MUSCLE_LOG_DEBUG, "Source [%s] is now online [%s].\n", source.ToString()(), newHB()->ToString()());
   }
}

// This method may be called by the main thread!  Hence the MutexGuard
uint64 PZGHeartbeatThreadState :: GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const
{
   DECLARE_MUTEXGUARD(_mainThreadLatenciesLock);
   return _mainThreadLatencies.GetWithDefault(peerID, MUSCLE_TIME_NEVER);
}

};  // end namespace zg_private

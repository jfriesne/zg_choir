#ifndef PZGDatabaseState_h
#define PZGDatabaseState_h

#include "zg/private/PZGNameSpace.h"
#include "zg/private/PZGDatabaseStateInfo.h"
#include "zg/private/PZGDatabaseUpdate.h"
#include "zg/private/PZGUpdateBackOrderKey.h"
#include "util/NestCount.h"
#include "util/PulseNode.h"

namespace zg
{
// forward declarations
class INetworkTimeProvider;
class ZGPeerID;
class ZGPeerSession;
};

namespace zg_private
{

/** This class represents the current state of a single replicated database.  */
class PZGDatabaseState : public PulseNode
{
public:
   PZGDatabaseState();

   void SetParameters(ZGPeerSession * master, uint32 whichDatabase, uint64 maxPayloadBytesInLog);

   status_t HandleDatabaseUpdateRequest(const ZGPeerID & fromPeerID, const ConstMessageRef & msg, const ConstPZGDatabaseUpdateRef & optDBUp, const INetworkTimeProvider & networkTimeProvider);

   MUSCLE_NODISCARD virtual uint64 GetPulseTime(const PulseArgs & args) {return muscleMin(_rescanLogPending?0:MUSCLE_TIME_NEVER, PulseNode::GetPulseTime(args));}
   virtual void Pulse(const PulseArgs & args);

   void PrintDatabaseStateInfo() const;
   void PrintDatabaseUpdateLog() const;

   PZGDatabaseStateInfo GetDatabaseStateInfo() const;

   void SeniorDatabaseStateInfoChanged(const PZGDatabaseStateInfo & seniorDBInfo);

   void ScheduleLogContentsRescan();
   void RescanUpdateLogIfNecessary();

   void BackOrderResultReceived(const PZGUpdateBackOrderKey & ubok, const ConstPZGDatabaseUpdateRef & optUpdateData);
   ConstPZGDatabaseUpdateRef GetDatabaseUpdateByID(uint64 updateID, const INetworkTimeProvider & networkTimeProvider) const;
   ConstMessageRef GetDatabaseUpdatePayloadByID(uint64 updateID) const;

   MUSCLE_NODISCARD bool IsInJuniorDatabaseUpdateContext(uint64 * optRetSeniorNetworkTime64) const
   {
      const bool ret = _inJuniorDatabaseUpdate.IsInBatch();
      if (optRetSeniorNetworkTime64) *optRetSeniorNetworkTime64 = ret ? _seniorUpdateTimeForJuniorUpdate : 0;
      return ret;
   }

   MUSCLE_NODISCARD bool IsInSeniorDatabaseUpdateContext() const {return _inSeniorDatabaseUpdate.IsInBatch();}

   MUSCLE_NODISCARD bool UpdateLogContainsUpdate(uint64 tid) const {return _updateLog.ContainsKey(tid);}
   MUSCLE_NODISCARD uint64 GetCurrentDatabaseStateID() const {return _localDatabaseStateID;}

   void ResetLocalDatabaseToDefaultState();
   void VerifyOrFixLocalDatabaseChecksum();

private:
   void RescanUpdateLog();
   status_t AddDatabaseUpdateToUpdateLog(const ConstPZGDatabaseUpdateRef & dbUp);
   void RemoveDatabaseUpdateFromUpdateLog(const ConstPZGDatabaseUpdateRef & dbUp);
   void ClearUpdateLog();
   void SeniorUpdateCompleted(const PZGDatabaseUpdateRef & dbUp, uint64 startTime, const ConstMessageRef & payloadMsg, const INetworkTimeProvider & networkTimeProvider);

   status_t RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok, bool dueToChecksumError);
   MUSCLE_NODISCARD uint64 GetTargetDatabaseStateID() const {return muscleMax(_updateLog.GetLastKeyWithDefault(), _seniorDatabaseStateID);}
   MUSCLE_NODISCARD bool IsDatabaseUpdateStillNeededToAdvanceJuniorPeerState(uint64 databaseUpdateID) const;

   status_t JuniorExecuteDatabaseReplace(const PZGDatabaseUpdate & dbUp);
   status_t JuniorExecuteDatabaseUpdate(const PZGDatabaseUpdate & dbUp);
   status_t JuniorExecuteDatabaseUpdateAux(const PZGDatabaseUpdate & dbUp);
   void JuniorPeerNeedsMissingUpdate(uint64 missingStateID);
   status_t RequestFullDatabaseResendFromSeniorPeer(bool dueToChecksumError);
   MUSCLE_NODISCARD bool IsAwaitingFullDatabaseResendReply() const;

   ZGPeerSession * _master;
   uint32 _whichDatabase;

   OrderedKeysHashtable<uint64, ConstPZGDatabaseUpdateRef> _updateLog;  // update ID -> update date, for recent updates
   uint64 _maxPayloadBytesInLog;     // we should start trimming the log when (_totalPayloadBytesInLog > _maxPayloadBytesInLog)
   uint64 _totalPayloadBytesInLog;   // always set to be equal to the total number of message-bytes in the log
   uint64 _totalElapsedMillisInLog;  // always set to be equal to the total milliseconds of all updates currently in the _updateLog
   uint64 _localDatabaseStateID;     // ID of the state our own local copy of the database is currently in
   uint64 _seniorDatabaseStateID;    // the senior peer's current database state ID (according to the most recent beacon packet we received from him)
   uint64 _seniorOldestIDInLog;      // the lowest database state update ID that the senior peer still has in his update log
   bool _seniorDatabaseStateReceived;  // true iff we've received at least one beacon (used by juniors only)
   uint32 _dbChecksum;               // running checksum of the contents of this database
   uint64 _firstUnsentUpdateID;      // ID of the first update in our log that we haven't sent out to multicast yet
   bool _rescanLogPending;           // dirty-flag, true iff the _updateLog's contents have changed and we need to act on the new contents
   bool _printDatabaseStatesComparisonOnNextReplace;  // for easier debugging

   Hashtable<PZGUpdateBackOrderKey, Void> _backorders;  // update-resends we have on order from the senior peer

   NestCount _inJuniorDatabaseUpdate;
   NestCount _inSeniorDatabaseUpdate;

   uint64 _seniorUpdateTimeForJuniorUpdate;  // only meaningful when we're inside JuniorExecuteDatabaseUpdateAux()
};

};  // end namespace zg_private

#endif

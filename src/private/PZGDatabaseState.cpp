#include "zg/private/PZGDatabaseState.h"
#include "zg/private/PZGConstants.h"
#include "zg/ZGPeerSession.h"

namespace zg_private
{

PZGDatabaseState :: PZGDatabaseState()
   : _master(NULL)
   , _whichDatabase((uint32)-1)
   , _maxPayloadBytesInLog(0)
   , _totalPayloadBytesInLog(0)
   , _totalElapsedMillisInLog(0)
   , _localDatabaseStateID(0)
   , _seniorDatabaseStateID(0)
   , _seniorOldestIDInLog((uint64)-1)
   , _seniorDatabaseStateReceived(false)
   , _dbChecksum(0)
   , _firstUnsentUpdateID(0)
   , _rescanLogPending(false)
   , _printDatabaseStatesComparisonOnNextReplace(false)
{
   // empty
}

void PZGDatabaseState :: SetParameters(ZGPeerSession * master, uint32 whichDatabase, uint64 maxPayloadBytesInLog) 
{
   _master               = master; 
   _whichDatabase        = whichDatabase;
   _maxPayloadBytesInLog = maxPayloadBytesInLog;
}

void PZGDatabaseState :: ScheduleLogContentsRescan()
{
   if ((_rescanLogPending == false)&&(_master->IAmFullyAttached()))
   {
      _rescanLogPending = true;
      InvalidatePulseTime();
   }
}

status_t PZGDatabaseState :: AddDatabaseUpdateToUpdateLog(const ConstPZGDatabaseUpdateRef & dbUp)
{
   const bool logWasEmpty = _updateLog.IsEmpty();
   if ((dbUp() == NULL)||(_updateLog.Put(dbUp()->GetUpdateID(), dbUp) != B_NO_ERROR)) return B_ERROR;

   const ConstByteBufferRef & payloadBuf = dbUp()->GetPayloadBuffer();
   if (payloadBuf()) _totalPayloadBytesInLog += payloadBuf()->GetNumBytes();

   _totalElapsedMillisInLog += dbUp()->GetSeniorElapsedTimeMillis();

   if ((logWasEmpty)&&(_master->IAmTheSeniorPeer())) _seniorOldestIDInLog = dbUp()->GetUpdateID();  // probably not necessary but I like to keep it correct
   ScheduleLogContentsRescan();
   return B_NO_ERROR;
}

void PZGDatabaseState :: RemoveDatabaseUpdateFromUpdateLog(const ConstPZGDatabaseUpdateRef & dbUp)
{
   ConstPZGDatabaseUpdateRef temp;  // necessary to avoid freeing the object before we're done using it, below
   if (_updateLog.Remove(dbUp()->GetUpdateID(), temp) == B_NO_ERROR)
   {
      const ConstByteBufferRef & payloadBuf = temp()->GetPayloadBuffer();
      if (payloadBuf()) _totalPayloadBytesInLog -= payloadBuf()->GetNumBytes();

      _totalElapsedMillisInLog -= temp()->GetSeniorElapsedTimeMillis();

      if ((_updateLog.IsEmpty())&&(_master->IAmTheSeniorPeer())) _seniorOldestIDInLog = (uint64)-1;  // probably not necessary but I like to keep it correct
   }
}

void PZGDatabaseState :: ClearUpdateLog()
{
   _updateLog.Clear();
   _totalPayloadBytesInLog  = 0;
   _totalElapsedMillisInLog = 0;
}

void PZGDatabaseState :: SeniorUpdateCompleted(const PZGDatabaseUpdateRef & dbUp, uint64 startTime, const ConstMessageRef & payloadMsg)
{
   // Gotta update our running time and byte tallies as we update dbUp
   _totalElapsedMillisInLog -= dbUp()->GetSeniorElapsedTimeMillis();
   dbUp()->SetSeniorElapsedTimeMicros(GetRunTime64()-startTime);
   _totalElapsedMillisInLog += dbUp()->GetSeniorElapsedTimeMillis();

   dbUp()->SetPostUpdateDBChecksum(_dbChecksum);

   if (payloadMsg() != dbUp()->GetPayloadBufferAsMessage()())
   {
      const ConstByteBufferRef & oldPayloadBuf = dbUp()->GetPayloadBuffer();
      if (oldPayloadBuf()) _totalPayloadBytesInLog -= oldPayloadBuf()->GetNumBytes();

      dbUp()->SetPayloadMessage(payloadMsg);

      const ConstByteBufferRef & newPayloadBuf = dbUp()->GetPayloadBuffer();
      if (newPayloadBuf()) _totalPayloadBytesInLog += newPayloadBuf()->GetNumBytes();
   }

   _seniorDatabaseStateID = ++_localDatabaseStateID;
   _master->ScheduleSetBeaconData();
}

// Note:  This method gets called only from within ZGPeerSession.cpp, and that code will make sure that
// this method gets called only in the correct context (e.g. it will make sure not to call
// PZG_PEER_COMMAND_RESET_SENIOR_DATABASE when we're running on a junior peer, etc)
// So we don't do that checking here.
status_t PZGDatabaseState :: HandleDatabaseUpdateRequest(const ZGPeerID & fromPeerID, const MessageRef & msg, const ConstPZGDatabaseUpdateRef & optDBUp)
{
   switch(msg()->what)
   {
      case PZG_PEER_COMMAND_RESET_SENIOR_DATABASE:
      {
         PZGDatabaseUpdateRef dbUp = GetPZGDatabaseUpdateFromPool(PZG_DATABASE_UPDATE_TYPE_RESET, _whichDatabase, _localDatabaseStateID+1, fromPeerID, _dbChecksum);
         if (dbUp() == NULL) return B_ERROR;
         if (AddDatabaseUpdateToUpdateLog(dbUp) != B_NO_ERROR) return B_ERROR;

         const uint64 startTime = GetRunTime64();
         {
            NestCountGuard ncg(_inSeniorDatabaseUpdate);
            (void) _master->ResetLocalDatabaseToDefault(_whichDatabase, _dbChecksum);
         }
         SeniorUpdateCompleted(dbUp, startTime, ConstMessageRef());
         return B_NO_ERROR;
      }
      break;

      case PZG_PEER_COMMAND_REPLACE_SENIOR_DATABASE:
      {
         MessageRef userDBStateMsg = msg()->GetMessage(PZG_PEER_NAME_USER_MESSAGE);
         if (userDBStateMsg() == NULL)
         {
            LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdateState:  Error, no user message to replace senior database #" UINT32_FORMAT_SPEC "!\n", _whichDatabase);
            return B_ERROR;
         }

         PZGDatabaseUpdateRef dbUp = GetPZGDatabaseUpdateFromPool(PZG_DATABASE_UPDATE_TYPE_REPLACE, _whichDatabase, _localDatabaseStateID+1, fromPeerID, _dbChecksum);
         if (dbUp() == NULL) return B_ERROR;

         if (AddDatabaseUpdateToUpdateLog(dbUp) != B_NO_ERROR) return B_ERROR;

         const uint64 startTime = GetRunTime64();
         status_t ret;
         {
            NestCountGuard ncg(_inSeniorDatabaseUpdate);
            ret = _master->SetLocalDatabaseFromMessage(_whichDatabase, _dbChecksum, userDBStateMsg);
         }
     
         if (ret.IsOK())
         {
            SeniorUpdateCompleted(dbUp, startTime, userDBStateMsg);
            return B_NO_ERROR;
         }
         else
         {
            LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdateState:  Error setting senior database #" UINT32_FORMAT_SPEC " to state! [%s]\n", _whichDatabase, ret());
            RemoveDatabaseUpdateFromUpdateLog(dbUp);  // roll back!
            return B_ERROR;
         }
      }
      break;

      case PZG_PEER_COMMAND_UPDATE_SENIOR_DATABASE:
      {
         MessageRef userDBUpdateMsg = msg()->GetMessage(PZG_PEER_NAME_USER_MESSAGE);
         if (userDBUpdateMsg() == NULL)
         {
            LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdateState:  Error, no user message to update senior database #" UINT32_FORMAT_SPEC "!\n", _whichDatabase);
            return B_ERROR;
         }

         PZGDatabaseUpdateRef dbUp = GetPZGDatabaseUpdateFromPool(PZG_DATABASE_UPDATE_TYPE_UPDATE, _whichDatabase, _localDatabaseStateID+1, fromPeerID, _dbChecksum);
         if (dbUp() == NULL) return B_ERROR;

         if (AddDatabaseUpdateToUpdateLog(dbUp) != B_NO_ERROR) return B_ERROR;

         const uint64 startTime = GetRunTime64();
         ConstMessageRef juniorMsg;
         {
            NestCountGuard ncg(_inSeniorDatabaseUpdate);
            juniorMsg = _master->SeniorUpdateLocalDatabase(_whichDatabase, _dbChecksum, userDBUpdateMsg);
         }

         if (juniorMsg())
         {
            SeniorUpdateCompleted(dbUp, startTime, juniorMsg);
            return B_NO_ERROR;
         }
         else
         {
            LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdateState:  Error setting senior database #" UINT32_FORMAT_SPEC " to state!\n", _whichDatabase);
            RemoveDatabaseUpdateFromUpdateLog(dbUp);  // roll back!
            return B_ERROR;
         }
      }
      break;

      case PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE:
      {
         if (optDBUp())
         {
            if (AddDatabaseUpdateToUpdateLog(optDBUp) != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseState::HandleDatabaseUpdateRequest:  Unable to add junior update to update log!\n");
         } 
         else LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseState::HandleDatabaseUpdateRequest:  No PZGDatabaseUpdate provided in Message " UINT32_FORMAT_SPEC "\n", msg()->what);
      }
      return B_ERROR;

      default:
         LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseState::HandleDatabaseUpdateRequest:  Unknown what code in Message " UINT32_FORMAT_SPEC "\n", msg()->what);
      return B_ERROR;
   }
}

void PZGDatabaseState :: Pulse(const PulseArgs & args)
{
   PulseNode::Pulse(args);
   RescanUpdateLogIfNecessary();
}

void PZGDatabaseState :: RescanUpdateLogIfNecessary()
{
   if (_rescanLogPending)
   {
      _rescanLogPending = false;
      RescanUpdateLog();
   }
}

void PZGDatabaseState :: RescanUpdateLog()
{
   if (_master->IAmTheSeniorPeer())
   {
      if (_updateLog.HasItems())
      {
         // Start iterating backwards from the end of the table (i.e. the most recent updates) until we get past all the unsent ones
         const uint64 oldestUpdateID = *_updateLog.GetFirstKey();
         for (HashtableIterator<uint64, ConstPZGDatabaseUpdateRef> iter(_updateLog, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
         {
            const uint64 nextUpdateID = iter.GetKey();

            // Stage 1: When we get to the first already-sent update, then we'll re-reverse the iterator so that it iterates forwards again
            if ((iter.IsBackwards())&&((nextUpdateID <= _firstUnsentUpdateID)||(nextUpdateID == oldestUpdateID))) iter.SetBackwards(false);

            // Stage 2: When iterating forwards, we'll send every unsent-update we encounter
            if ((!iter.IsBackwards())&&(nextUpdateID >= _firstUnsentUpdateID)&&(_master->SendDatabaseUpdateViaMulticast(iter.GetValue()) == B_NO_ERROR)) _firstUnsentUpdateID = nextUpdateID+1;
         }

         // Finally, let's trim old ConstPZGDatabaseUpdates from our _updateLog if necessary, until it again fits within our memory budget
         while((_totalPayloadBytesInLog > _maxPayloadBytesInLog)&&(_updateLog.GetNumItems() > 1)) RemoveDatabaseUpdateFromUpdateLog(*_updateLog.GetFirstValue());
      }
   }
   else if (_seniorDatabaseStateReceived)  // no point trying to scan if we don't know where we want to scan to!
   {
      _firstUnsentUpdateID = _localDatabaseStateID+1; // as a junior we don't really use this, but update it anyway, in case we become senior later

      if (IsAwaitingFullDatabaseResendReply() == false)  // no point replaying our log if we're waiting for the full DB anyway
      {
         NestCountGuard ncg(_inJuniorDatabaseUpdate);

         // What we want to do is try to move our database state forward as much as possible, hopefully
         // until it matches the current state of the senior peer.  Ideally we have all the PZGDatabaseUpdates
         // in our update-log that will tell us how to do this, but if not (e.g. because we didn't
         // receive the multicast packets for whatever reason), we may have to request a resend of
         // the missing PZGDatabaseUpdates from the senior peer, and if that doesn't work, our 
         // ultimate fallback will be to request the full state of the current database from the senior peer.
         const uint64 targetDatabaseStateID = GetTargetDatabaseStateID();
         while(_localDatabaseStateID < targetDatabaseStateID)
         {
            const uint64 nextStateID = _localDatabaseStateID+1;
            ConstPZGDatabaseUpdateRef dbUp = _updateLog[nextStateID];
            if (dbUp())
            {
               if (JuniorExecuteDatabaseUpdate(*dbUp()) == B_NO_ERROR)
               {
                  LogTime(MUSCLE_LOG_DEBUG, "Database #" UINT32_FORMAT_SPEC " successfully executed junior update to state #" UINT64_FORMAT_SPEC "\n", _whichDatabase, nextStateID);
               }
               else
               {
                  LogTime(MUSCLE_LOG_ERROR, "Database #" UINT32_FORMAT_SPEC " was unable to execute junior update #" UINT64_FORMAT_SPEC ", will try to recover by requesting full database resend.\n", _whichDatabase, nextStateID);
                  if (RequestFullDatabaseResendFromSeniorPeer() != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "Request for full database resend failed!\n");
                  break;
               }
            } 
            else 
            {
               const ZGPeerID & seniorPeerID = _master->GetSeniorPeerID();
               if (seniorPeerID.IsValid())
               {
                  if (nextStateID < _seniorOldestIDInLog)
                  {
                     LogTime(MUSCLE_LOG_DEBUG, "Next required state ID " UINT64_FORMAT_SPEC " is no longer in senior peer's log (oldest he has is " UINT64_FORMAT_SPEC "), so we'll request a full DB #" UINT32_FORMAT_SPEC " resend instead.\n", nextStateID, _seniorOldestIDInLog, _whichDatabase);
                     if (RequestFullDatabaseResendFromSeniorPeer() != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "Request for full database resend failed!\n");
                  }
                  else
                  {
                     // Oops, we can't update our local DB any further (for now), but we can at least make sure
                     // that the PZGDatabaseUpdates we need are on back-order from the senior peer
                     for (uint64 updateID=nextStateID; updateID<=targetDatabaseStateID; updateID++)
                     {
                        if (_updateLog.ContainsKey(updateID) == false)
                        {
                           const PZGUpdateBackOrderKey ubok(seniorPeerID, _whichDatabase, updateID);
                           if (_backorders.ContainsKey(ubok) == false)
                           {
                              if (RequestBackOrderFromSeniorPeer(ubok) == B_NO_ERROR)
                              {
                                 LogTime(MUSCLE_LOG_DEBUG, "Database " UINT32_FORMAT_SPEC ":  Placed update #" UINT64_FORMAT_SPEC " on back-order from senior peer [%s]\n", ubok.GetDatabaseIndex(), ubok.GetDatabaseUpdateID(), ubok.GetTargetPeerID().ToString()());
                              }
                              else
                              {
                                 LogTime(MUSCLE_LOG_ERROR, "Database " UINT32_FORMAT_SPEC ":  Requested back order of update #" UINT64_FORMAT_SPEC " failed, requesting full resend\n", ubok.GetDatabaseIndex(), ubok.GetDatabaseUpdateID());
                                 if (RequestFullDatabaseResendFromSeniorPeer() != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "Request for full database resend failed!\n");
                                 break;
                              }
                           }
                        }
                     }
                  }
               }
               break;
            }
         }
      }

      // Finally, let's trim old/unneeded ConstPZGDatabaseUpdates from our _updateLog if necessary, until it again fits within our memory budget
      while((_totalPayloadBytesInLog > _maxPayloadBytesInLog)&&(_updateLog.GetNumItems() > 1)&&(IsDatabaseUpdateStillNeededToAdvanceJuniorPeerState(*_updateLog.GetFirstKey()) == false)) RemoveDatabaseUpdateFromUpdateLog(*_updateLog.GetFirstValue());
   }
}

status_t PZGDatabaseState :: RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok)
{
   if (_backorders.ContainsKey(ubok)) return B_NO_ERROR;  // paranoia:  it's already on order, no need to ask again

   if (_backorders.PutWithDefault(ubok) == B_NO_ERROR)
   {
      if (_master->RequestBackOrderFromSeniorPeer(ubok) == B_NO_ERROR) return B_NO_ERROR;
      (void) _backorders.Remove(ubok);  // roll back!
   }
   return B_ERROR;
}

status_t PZGDatabaseState :: RequestFullDatabaseResendFromSeniorPeer()
{
   return RequestBackOrderFromSeniorPeer(PZGUpdateBackOrderKey(_master->GetSeniorPeerID(), _whichDatabase, DATABASE_UPDATE_ID_FULL_UPDATE));
}

bool PZGDatabaseState :: IsAwaitingFullDatabaseResendReply() const
{
   return (_backorders.ContainsKey(PZGUpdateBackOrderKey(_master->GetSeniorPeerID(), _whichDatabase, DATABASE_UPDATE_ID_FULL_UPDATE)));
}

status_t PZGDatabaseState :: JuniorExecuteDatabaseUpdate(const PZGDatabaseUpdate & dbUp)
{
   const uint64 newDatabaseStateID = dbUp.GetUpdateID();
   if ((_localDatabaseStateID+1) != newDatabaseStateID)
   {
      // This should be unnecessary since the calling code's logic should have already guaranteed this, but I'm paranoid
      LogTime(MUSCLE_LOG_ERROR, "Error, junior update #" UINT64_FORMAT_SPEC " isn't the right update to advance current state " UINT64_FORMAT_SPEC " of database #" UINT32_FORMAT_SPEC "\n", _dbChecksum, _whichDatabase, dbUp.GetPreUpdateDBChecksum(), newDatabaseStateID);
      return B_ERROR;
   }
   if (_dbChecksum != dbUp.GetPreUpdateDBChecksum())
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, DB checksum " UINT32_FORMAT_SPEC " of database #" UINT32_FORMAT_SPEC " doesn't match required pre-update DB checksum " UINT32_FORMAT_SPEC " for junior update #" UINT64_FORMAT_SPEC "\n", _dbChecksum, _whichDatabase, dbUp.GetPreUpdateDBChecksum(), newDatabaseStateID);
      String dbContents = _master->GetLocalDatabaseContentsAsString(_whichDatabase);
      if (dbContents.HasChars()) printf("Mismatched Local pre-update state was:\n%s\n", dbContents());
      return B_ERROR;
   }
   if (JuniorExecuteDatabaseUpdateAux(dbUp) != B_NO_ERROR) return B_ERROR;
   if (_dbChecksum != dbUp.GetPostUpdateDBChecksum())
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, DB checksum " UINT32_FORMAT_SPEC " of database #" UINT32_FORMAT_SPEC " doesn't match required post-update DB checksum " UINT32_FORMAT_SPEC " for junior update #" UINT64_FORMAT_SPEC "\n", _dbChecksum, _whichDatabase, dbUp.GetPostUpdateDBChecksum(), newDatabaseStateID);
      String dbContents = _master->GetLocalDatabaseContentsAsString(_whichDatabase);
      if (dbContents.HasChars()) printf("Mismatched Local post-update state was:\n%s\n", dbContents());

      _printDatabaseStatesComparisonOnNextReplace = true;  // so we can more easily debug what went wrong
      return B_ERROR;
   }

   _localDatabaseStateID = newDatabaseStateID;
   return B_NO_ERROR;  // success!
} 

status_t PZGDatabaseState :: JuniorExecuteDatabaseReplace(const PZGDatabaseUpdate & dbUp)
{
   const bool doPrints = _printDatabaseStatesComparisonOnNextReplace;
   _printDatabaseStatesComparisonOnNextReplace = false;  // clear this now so that if we return early it will still be cleared
   if (doPrints)
   {
      LogTime(MUSCLE_LOG_WARNING, "JuniorExecuteDatabaseReplace(#" UINT32_FORMAT_SPEC "):  pre-update local checksum is " UINT32_FORMAT_SPEC " (recalc=" UINT32_FORMAT_SPEC "), dbUp=[%s]\n", _whichDatabase, _dbChecksum, _master->CalculateLocalDatabaseChecksum(_whichDatabase), dbUp.ToString()());
      String dbStr = _master->GetLocalDatabaseContentsAsString(_whichDatabase);
      if (dbStr.HasChars()) printf("Contents of database #" UINT32_FORMAT_SPEC " before the DB-replace are:\n\n%s\n", _whichDatabase, dbStr());
   }

   if (JuniorExecuteDatabaseUpdateAux(dbUp) != B_NO_ERROR) return B_ERROR;

   if (doPrints)
   {
      LogTime(MUSCLE_LOG_WARNING, "JuniorExecuteDatabaseReplace(#" UINT32_FORMAT_SPEC "):  post-update local checksum is " UINT32_FORMAT_SPEC " (recalc=" UINT32_FORMAT_SPEC "), dbUp=[%s]\n", _whichDatabase, _dbChecksum, _master->CalculateLocalDatabaseChecksum(_whichDatabase), dbUp.ToString()());
      String dbStr = _master->GetLocalDatabaseContentsAsString(_whichDatabase);
      if (dbStr.HasChars()) printf("Contents of database #" UINT32_FORMAT_SPEC " after the DB-replace are:\n\n%s\n", _whichDatabase, dbStr());
   }

   const uint64 newDatabaseStateID = dbUp.GetUpdateID();
   if (_dbChecksum != dbUp.GetPostUpdateDBChecksum())
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, DB checksum " UINT32_FORMAT_SPEC " of database #" UINT32_FORMAT_SPEC " doesn't match required post-replace DB checksum " UINT32_FORMAT_SPEC " for junior replace #" UINT64_FORMAT_SPEC "\n", _dbChecksum, _whichDatabase, dbUp.GetPostUpdateDBChecksum(), newDatabaseStateID);
      return B_ERROR;
   }

   _localDatabaseStateID = newDatabaseStateID;
   LogTime(MUSCLE_LOG_DEBUG, "Junior database #" UINT32_FORMAT_SPEC " is now replaced by the senior database at state #" UINT64_FORMAT_SPEC "\n", _whichDatabase, _localDatabaseStateID);
   return B_NO_ERROR;   
}
   
status_t PZGDatabaseState :: JuniorExecuteDatabaseUpdateAux(const PZGDatabaseUpdate & dbUp)
{
   switch(dbUp.GetUpdateType())
   {
      case PZG_DATABASE_UPDATE_TYPE_NOOP:
         return B_NO_ERROR;  // that was easy!

      case PZG_DATABASE_UPDATE_TYPE_RESET:
         (void) _master->ResetLocalDatabaseToDefault(_whichDatabase, _dbChecksum);
         return B_NO_ERROR;

      case PZG_DATABASE_UPDATE_TYPE_REPLACE:
      {
         const ConstMessageRef & userDBStateMsg = dbUp.GetPayloadBufferAsMessage();
         if (userDBStateMsg() == NULL)
         {
            LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdateState:  Error, no user message available to replace junior database #" UINT32_FORMAT_SPEC "!\n", _whichDatabase);
            return B_ERROR;
         }

         status_t ret = _master->SetLocalDatabaseFromMessage(_whichDatabase, _dbChecksum, userDBStateMsg);
         dbUp.UncachePayloadBufferAsMessage();  // might as well free up the memory, now that we've executed it we won't need the Message again
         return ret;
      }

      case PZG_DATABASE_UPDATE_TYPE_UPDATE:
      {
         const ConstMessageRef & userDBUpdateMsg = dbUp.GetPayloadBufferAsMessage();
         if (userDBUpdateMsg() == NULL)
         {
            LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseUpdateState:  Error, no user message to update junior database #" UINT32_FORMAT_SPEC "!\n", _whichDatabase);
            return B_ERROR;
         }

         status_t ret = _master->JuniorUpdateLocalDatabase(_whichDatabase, _dbChecksum, userDBUpdateMsg);
         dbUp.UncachePayloadBufferAsMessage();  // might as well free up the memory, now that we've executed it we won't need the Message again
         return ret;
      }

      default:
         LogTime(MUSCLE_LOG_ERROR, "PZGDatabaseState::JuniorExecuteDatabaseUpdateAux:  Unknown update type code " UINT32_FORMAT_SPEC "\n", dbUp.GetUpdateType());
      return B_ERROR;
   }
}

void PZGDatabaseState :: PrintDatabaseStateInfo() const
{
   const uint32 recalculatedChecksum = _master->CalculateLocalDatabaseChecksum(_whichDatabase);
   char buf[128] = "";
   if (recalculatedChecksum == _dbChecksum) muscleSprintf(buf, "checksum=" UINT32_FORMAT_SPEC, _dbChecksum);
                                       else muscleSprintf(buf, "[[[ERROR running DB checksum is " UINT32_FORMAT_SPEC ", but recalculated checksum is " UINT32_FORMAT_SPEC "]]]", _dbChecksum, recalculatedChecksum);

   printf("DB #" UINT32_FORMAT_SPEC ":  UpdateLog has " UINT32_FORMAT_SPEC " items (" UINT64_FORMAT_SPEC "/" UINT64_FORMAT_SPEC " bytes, " UINT64_FORMAT_SPEC " millis), %s, state=" UINT64_FORMAT_SPEC ", FirstUnsentID=" UINT64_FORMAT_SPEC "\n", _whichDatabase, _updateLog.GetNumItems(), _totalPayloadBytesInLog, _maxPayloadBytesInLog, _totalElapsedMillisInLog, buf, _localDatabaseStateID, _firstUnsentUpdateID);  
}

void PZGDatabaseState :: PrintDatabaseUpdateLog() const
{
   printf("Update log for database #" UINT32_FORMAT_SPEC " has " UINT32_FORMAT_SPEC " items (" UINT64_FORMAT_SPEC "/" UINT64_FORMAT_SPEC " bytes, " UINT64_FORMAT_SPEC " milliseconds):\n", _whichDatabase, _updateLog.GetNumItems(), _totalPayloadBytesInLog, _maxPayloadBytesInLog, _totalElapsedMillisInLog);
   for (HashtableIterator<uint64, ConstPZGDatabaseUpdateRef> iter(_updateLog); iter.HasData(); iter++) printf("  %s\n", iter.GetValue()()->ToString()());
}

PZGDatabaseStateInfo PZGDatabaseState :: GetDatabaseStateInfo() const
{
   return PZGDatabaseStateInfo(_localDatabaseStateID, _updateLog.GetFirstKeyWithDefault((uint64)-1), _dbChecksum);
}

void PZGDatabaseState :: SeniorDatabaseStateInfoChanged(const PZGDatabaseStateInfo & seniorDBInfo)
{
   const uint64 seniorState         = seniorDBInfo.GetCurrentDatabaseStateID();
   const uint64 seniorOldestIDInLog = seniorDBInfo.GetOldestDatabaseIDInLog();
   if ((seniorState != _seniorDatabaseStateID)||(seniorOldestIDInLog != _seniorOldestIDInLog))
   {
      _seniorDatabaseStateReceived = (_seniorDatabaseStateID != (uint64)-1);
      _seniorDatabaseStateID = seniorState;
      _seniorOldestIDInLog   = seniorOldestIDInLog;
      ScheduleLogContentsRescan();  // this will verify that we are up-to-date vis-a-vis the new senior state (or cause us to take steps to become so if we aren't)
   }
}

void PZGDatabaseState :: BackOrderResultReceived(const PZGUpdateBackOrderKey & ubok, const ConstPZGDatabaseUpdateRef & optUpdateData)
{
   const ZGPeerID & seniorPeerID = _master->GetSeniorPeerID();
   if ((_backorders.Remove(ubok) == B_NO_ERROR)&&(ubok.GetTargetPeerID() == seniorPeerID)&&(_master->IAmTheSeniorPeer() == false))
   {
      if (ubok.GetDatabaseUpdateID() == DATABASE_UPDATE_ID_FULL_UPDATE)
      {
         if (optUpdateData())
         {
            LogTime(MUSCLE_LOG_DEBUG, "Database #" UINT32_FORMAT_SPEC ":  Received full database state from senior peer.\n", _whichDatabase);
            NestCountGuard ncg(_inJuniorDatabaseUpdate);
            if (JuniorExecuteDatabaseReplace(*optUpdateData()) == B_NO_ERROR) ScheduleLogContentsRescan();
         }
         else LogTime(MUSCLE_LOG_ERROR, "Database #" UINT32_FORMAT_SPEC ":  Senior peer failed to send full database state to us!\n", _whichDatabase, ubok.GetDatabaseUpdateID());  // now what do we do?
      }
      else
      {
         if ((optUpdateData())&&(_updateLog.Put(ubok.GetDatabaseUpdateID(), optUpdateData) == B_NO_ERROR))
         {
            LogTime(MUSCLE_LOG_DEBUG, "Database #" UINT32_FORMAT_SPEC ":  Back-order of database update #" UINT64_FORMAT_SPEC " received from senior peer.\n", _whichDatabase, ubok.GetDatabaseUpdateID());
            ScheduleLogContentsRescan();
         }
         else
         {
            LogTime(MUSCLE_LOG_WARNING, "Database #" UINT32_FORMAT_SPEC ":  Necessary back-order of database update #" UINT64_FORMAT_SPEC " failed, requesting full database to recover.\n", _whichDatabase, ubok.GetDatabaseUpdateID());
            if (RequestFullDatabaseResendFromSeniorPeer() != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "Request for full database resend failed!\n");
         }
      }
   }
}

bool PZGDatabaseState :: IsDatabaseUpdateStillNeededToAdvanceJuniorPeerState(uint64 databaseUpdateID) const
{
   if (databaseUpdateID <= _localDatabaseStateID)      return false;  // we already handled that one
   if (databaseUpdateID  > GetTargetDatabaseStateID()) return false;  // never heard of it
   if (IsAwaitingFullDatabaseResendReply())            return false;  // if we're going to get a full replacement, we don't need any updates
   return true;
}

ConstPZGDatabaseUpdateRef PZGDatabaseState :: GetDatabaseUpdateByID(uint64 updateID) const
{
   if (updateID == DATABASE_UPDATE_ID_FULL_UPDATE)
   {
      // For this special value we'll save our full current database state and return that
      const uint64 startTime = GetRunTime64();
      MessageRef savedDBMsg = _master->SaveLocalDatabaseToMessage(_whichDatabase);
      if (savedDBMsg())
      {
         PZGDatabaseUpdateRef dbUp = GetPZGDatabaseUpdateFromPool(PZG_DATABASE_UPDATE_TYPE_REPLACE, _whichDatabase, _localDatabaseStateID, _master->GetLocalPeerID(), _dbChecksum);
         if (dbUp())
         {
            dbUp()->SetSeniorElapsedTimeMicros(GetRunTime64()-startTime);
            dbUp()->SetPostUpdateDBChecksum(_dbChecksum);
            dbUp()->SetPayloadMessage(savedDBMsg);
            return AddConstToRef(dbUp);
         }
         else WARN_OUT_OF_MEMORY;
      }
      else LogTime(MUSCLE_LOG_ERROR, "Unable to save state #" UINT64_FORMAT_SPEC " of local database #" UINT32_FORMAT_SPEC " to a Message to satisfy external request!\n", _localDatabaseStateID, _whichDatabase);

      return ConstPZGDatabaseUpdateRef();
   }
   else return _updateLog[updateID];
}

};  // end namespace zg_private

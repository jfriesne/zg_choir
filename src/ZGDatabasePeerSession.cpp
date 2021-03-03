#include "zg/ZGDatabasePeerSession.h"

namespace zg
{

enum {
   DBPEERSESSION_COMMAND_MESSAGEFORDBOBJECT = 1684172915, // 'dbps' 
};

static const String DBPEERSESSION_NAME_PAYLOAD     = "pay";  // Message field-name
static const String DBPEERSESSION_NAME_TARGETDBIDX = "tdb";  // int32 field-name
static const String DBPEERSESSION_NAME_SOURCEDBIDX = "sdb";  // int32 field-name

ZGDatabasePeerSession :: ZGDatabasePeerSession(const ZGPeerSettings & zgPeerSettings) : ZGPeerSession(zgPeerSettings)
{
   // empty
}

status_t ZGDatabasePeerSession :: AttachedToServer()
{
   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   (void) _databaseObjects.EnsureSize(numDBs);
   for (uint32 i=0; i<numDBs; i++)
   {
      IDatabaseObjectRef dbRef = CreateDatabaseObject(i);
      if (dbRef())
      {
         status_t ret;
         if (_databaseObjects.AddTail(dbRef).IsError(ret)) return ret;
      }
      else
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "ZGDatabasePeerSession::AttachedToServer:  CreateDatabaseObject() failed for database #" UINT32_FORMAT_SPEC ", aborting startup!\n", i);
         return B_LOGIC_ERROR;
      }
   }

   return ZGPeerSession::AttachedToServer();  // must be done last!
}

void ZGDatabasePeerSession :: ResetLocalDatabaseToDefault(uint32 whichDatabase, uint32 & dbChecksum)
{
   IDatabaseObject * db = GetDatabaseObject(whichDatabase);
   db->SetToDefaultState();
   dbChecksum = db->GetCurrentChecksum();
}

ConstMessageRef ZGDatabasePeerSession :: SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg)
{
   IDatabaseObject * db = GetDatabaseObject(whichDatabase);
   ConstMessageRef ret = db->SeniorUpdate(seniorDoMsg);
   dbChecksum = db->GetCurrentChecksum();
   return ret;
}

status_t ZGDatabasePeerSession :: JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg)
{
   IDatabaseObject * db = GetDatabaseObject(whichDatabase);
   const status_t ret = db->JuniorUpdate(juniorDoMsg);
   dbChecksum = db->GetCurrentChecksum();
   return ret;
}

MessageRef ZGDatabasePeerSession :: SaveLocalDatabaseToMessage(uint32 whichDatabase) const
{
   MessageRef msg = GetMessageFromPool();
   if ((msg())&&(GetDatabaseObject(whichDatabase)->SaveToArchive(msg) != B_NO_ERROR)) msg.Reset();
   return msg;
}

status_t ZGDatabasePeerSession :: SetLocalDatabaseFromMessage(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & newDBStateMsg)
{
   IDatabaseObject * db = GetDatabaseObject(whichDatabase);
   status_t ret = db->SetFromArchive(newDBStateMsg);
   dbChecksum = db->GetCurrentChecksum();
   return ret;
}

uint32 ZGDatabasePeerSession :: CalculateLocalDatabaseChecksum(uint32 whichDatabase) const
{
   return GetDatabaseObject(whichDatabase)->CalculateChecksum();
}

String ZGDatabasePeerSession :: GetLocalDatabaseContentsAsString(uint32 whichDatabase) const
{
   return GetDatabaseObject(whichDatabase)->ToString();
}

void ZGDatabasePeerSession :: PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   ZGPeerSession::PeerHasGoneOffline(peerID, peerInfo);

   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++) _databaseObjects[i]()->PeerHasGoneOffline(peerID, peerInfo);
}

void ZGDatabasePeerSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   ZGPeerSession::PeerHasComeOnline(peerID, peerInfo);

   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++) _databaseObjects[i]()->PeerHasComeOnline(peerID, peerInfo);
}

void ZGDatabasePeerSession :: LocalSeniorPeerStatusChanged()
{
   ZGPeerSession::LocalSeniorPeerStatusChanged();

   const uint32 numDBs = GetPeerSettings().GetNumDatabases();
   for (uint32 i=0; i<numDBs; i++) _databaseObjects[i]()->LocalSeniorPeerStatusChanged();
}

status_t ZGDatabasePeerSession :: SendMessageToDatabaseObject(const ZGPeerID & targetPeerID, const MessageRef & msg, uint32 targetDBIdx, uint32 sourceDBIdx)
{
   MessageRef wrapperMsg = GetMessageFromPool(DBPEERSESSION_COMMAND_MESSAGEFORDBOBJECT);
   if (wrapperMsg() == NULL) MRETURN_OUT_OF_MEMORY;

   status_t ret = wrapperMsg()->AddMessage(DBPEERSESSION_NAME_PAYLOAD,     msg)
                | wrapperMsg()->CAddInt32( DBPEERSESSION_NAME_TARGETDBIDX, targetDBIdx)
                | wrapperMsg()->CAddInt32( DBPEERSESSION_NAME_SOURCEDBIDX, sourceDBIdx);
   if (ret.IsError()) return ret;

   return targetPeerID.IsValid() ? SendUnicastUserMessageToPeer(targetPeerID, wrapperMsg) : SendUnicastUserMessageToAllPeers(wrapperMsg);
}

void ZGDatabasePeerSession :: MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg)
{
   switch(msg()->what)
   {
      case DBPEERSESSION_COMMAND_MESSAGEFORDBOBJECT:
      {
         status_t ret;

         MessageRef payloadMsg;
         if (msg()->FindMessage(DBPEERSESSION_NAME_PAYLOAD, payloadMsg).IsOK(ret))
         {
            const uint32 targetDBIdx = msg()->GetInt32(DBPEERSESSION_NAME_TARGETDBIDX);
            if (targetDBIdx < _databaseObjects.GetNumItems())
            {
               _databaseObjects[targetDBIdx]()->MessageReceivedFromMessageTreeDatabaseObject(payloadMsg, fromPeerID, msg()->GetInt32(DBPEERSESSION_NAME_SOURCEDBIDX));
            }
            else LogTime(MUSCLE_LOG_ERROR, "MessageReceivedFromPeer:  invalid target database index " UINT32_FORMAT_SPEC "\n", targetDBIdx);
         }
         else LogTime(MUSCLE_LOG_ERROR, "MessageReceivedFromPeer:  Couldn't find payload in DBPEERSESSION_COMMAND_MESSAGEFORDBOBJECT Message!  [%s]\n", ret());
      }
      break;

      default:
         ZGPeerSession::MessageReceivedFromPeer(fromPeerID, msg);
      break;
   }
}

const IDatabaseObject * IDatabaseObject :: GetDatabaseObject(uint32 whichDatabase) const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetDatabaseObject(whichDatabase) : NULL;
}

bool IDatabaseObject :: IsPeerOnline(const ZGPeerID & pid) const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->IsPeerOnline(pid) : false;
}

const Hashtable<ZGPeerID, ConstMessageRef> & IDatabaseObject :: GetOnlinePeers() const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetOnlinePeers() : GetDefaultObjectForType< Hashtable<ZGPeerID, ConstMessageRef> >();
}

uint64 IDatabaseObject :: GetNetworkTime64() const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetNetworkTime64() : 0;
}

uint64 IDatabaseObject :: GetRunTime64ForNetworkTime64(uint64 networkTime64TimeStamp) const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetRunTime64ForNetworkTime64(networkTime64TimeStamp) : 0;
}

uint64 IDatabaseObject :: GetNetworkTime64ForRunTime64(uint64 runTime64TimeStamp) const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetNetworkTime64ForRunTime64(runTime64TimeStamp) : 0;
}

int64 IDatabaseObject :: GetToNetworkTimeOffset() const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetToNetworkTimeOffset() : 0;
}

status_t IDatabaseObject :: RequestResetDatabaseStateToDefault()
{
   ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->RequestResetDatabaseStateToDefault(_dbIndex) : B_BAD_OBJECT;
}

status_t IDatabaseObject :: RequestReplaceDatabaseState(const MessageRef & newDatabaseStateMsg)
{
   ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->RequestReplaceDatabaseState(_dbIndex, newDatabaseStateMsg) : B_BAD_OBJECT;
}

status_t IDatabaseObject :: RequestUpdateDatabaseState(const MessageRef & databaseUpdateMsg)
{
   ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->RequestUpdateDatabaseState(_dbIndex, databaseUpdateMsg) : B_BAD_OBJECT;
}

bool IDatabaseObject :: IsInSeniorDatabaseUpdateContext() const
{
   ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->IsInSeniorDatabaseUpdateContext(_dbIndex) : false;
}

bool IDatabaseObject :: IsInJuniorDatabaseUpdateContext() const
{
   ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->IsInJuniorDatabaseUpdateContext(_dbIndex) : false;
}

status_t IDatabaseObject :: SendMessageToDatabaseObject(const ZGPeerID & targetPeerID, const MessageRef & msg, int32 optWhichDB)
{
   ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->SendMessageToDatabaseObject(targetPeerID, msg, (optWhichDB>=0)?(uint32)optWhichDB:_dbIndex, _dbIndex) : B_BAD_OBJECT;
}

uint64 IDatabaseObject :: GetCurrentDatabaseStateID() const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetCurrentDatabaseStateID(_dbIndex) : 0;
}

bool IDatabaseObject :: UpdateLogContainsUpdate(uint64 transactionID) const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->UpdateLogContainsUpdate(_dbIndex, transactionID) : 0;
}

ConstMessageRef IDatabaseObject :: GetUpdatePayload(uint64 transactionID) const
{
   const ZGDatabasePeerSession * dbps = GetDatabasePeerSession();
   return dbps ? dbps->GetUpdatePayload(_dbIndex, transactionID) : ConstMessageRef();
}

};  // end namespace zg

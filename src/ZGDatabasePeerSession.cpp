#include "zg/ZGDatabasePeerSession.h"

namespace zg
{

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
      if ((dbRef() == NULL)||(_databaseObjects.AddTail(dbRef) != B_NO_ERROR))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "ZGDatabasePeerSession::AttachedToServer:  CreateDatabaseObject() failed for database #" UINT32_FORMAT_SPEC ", aborting startup!\n", i);
         return B_ERROR;
      }
   }

   return ZGPeerSession::AttachedToServer();
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
printf(" SeniorUpdateLocalDatabase whichDatabase=%u db=%p msg=%p\n", whichDatabase, db, seniorDoMsg());
   ConstMessageRef ret = db->SeniorUpdate(seniorDoMsg);
   dbChecksum = db->GetCurrentChecksum();
   return ret;
}

status_t ZGDatabasePeerSession :: JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg)
{
   IDatabaseObject * db = GetDatabaseObject(whichDatabase);
   status_t ret = db->JuniorUpdate(juniorDoMsg);
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

};  // end namespace zg

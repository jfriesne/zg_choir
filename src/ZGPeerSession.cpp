#include "zg/ZGPeerSession.h"
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"  // for ZG_DISCOVERY_NAME_*
#include "zg/private/PZGConstants.h"
#include "zg/private/PZGHeartbeatSession.h"
#include "zg/private/PZGNetworkIOSession.h"
#include "reflector/StorageReflectSession.h"  // for PrintFactoriesInfo(), PrintSessionsInfo()
#include "system/Mutex.h"
#include "util/NetworkUtilityFunctions.h"     // for GetNetworkInterfaceInfos()

namespace zg_private
{
   extern void SetEnableTimeSynchronizationDebugging(bool enable);  // defined inside PZGHeartbeatThreadState.cpp
};

namespace zg
{

using namespace zg_private;

static uint32 GetNextUniqueObjectID()
{
   static Mutex _counterMutex;

   MutexGuard mg(_counterMutex);
   static uint32 _counter = 0;
   return ++_counter;
}

static ZGPeerID GenerateLocalPeerID()
{
   // Generates the Peer ID of this peer.  We want this Peer ID to be unique with respect
   // to all of the peers in this system, and it will be immutable for the lifetime of this
   // peer.  This is the ID by which this peer will be uniquely referenced by all peers in the system.

   // We'll start by finding a 48-bit MAC address on this machine, if we can.  MAC addresses are
   // unique to each network interface, so by including a MAC address as part of our ID, we
   // can guarantee that our ID will be different from any peer ID generated on a different
   // machine.
   uint64 macAddress = 0;
   {
      Queue<NetworkInterfaceInfo> niis;
      if (GetNetworkInterfaceInfos(niis) == B_NO_ERROR)
      {
         for (uint32 i=0; i<niis.GetNumItems(); i++)
         {
            const NetworkInterfaceInfo & nii = niis[i];
            if (nii.GetMACAddress() != 0) {macAddress = nii.GetMACAddress(); break;}
         }
      }
   }

   // But since we can (in some cases) have multiple ZGPeerSessions per host, the MAC address alone
   // isn't enough to guarantee uniqueness.  So we'll fill out the other 80 bits with values
   // that will be unique to this ZGPeerSession object.
#ifdef WIN32
   const uint32 processID = (uint32) GetCurrentProcessId();
#else
   const uint32 processID = (uint32) getpid();
#endif

   // And finally some additional salt just to make use of the other 32 low-bits.  This probably isn't necessary, but I'm paranoid
   unsigned int seed = (unsigned int) time(NULL);
   const uint32 salt = CalculateHashCode(GetCurrentTime64() + GetRunTime64() + GetRandomNumber(&seed));
   return ZGPeerID((macAddress<<16)|((uint64)GetNextUniqueObjectID()), (((uint64)processID)<<32)|((uint64)salt));
}

ZGPeerSession :: ZGPeerSession(const ZGPeerSettings & zgPeerSettings) : _peerSettings(zgPeerSettings), _localPeerID(GenerateLocalPeerID()), _iAmFullyAttached(false), _setBeaconDataPending(false)
{
   (void) _databases.EnsureSize(_peerSettings.GetNumDatabases(), true);
   for (uint32 i=0; i<_databases.GetNumItems(); i++) 
   {
      _databases[i].SetParameters(this, i, zgPeerSettings.GetMaximumUpdateLogSizeForDatabase(i));
      (void) PutPulseChild(&_databases[i]);  // So the PZGDatabaseState objects can use GetPulseTime() and Pulse() directly
   }
}

void ZGPeerSession :: MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg)
{
   LogTime(MUSCLE_LOG_WARNING, "ZGPeerSession::MessageReceivedFromPeer():  Unknown what code " UINT32_FORMAT_SPEC " in Message from [%s]\n", msg()->what, fromPeerID.ToString()());
}

status_t ZGPeerSession :: AttachedToServer()
{
   status_t ret;
   if (StorageReflectSession::AttachedToServer().IsError(ret)) return ret;

   PZGNetworkIOSessionRef ioSessionRef(newnothrow PZGNetworkIOSession(_peerSettings, _localPeerID, this));
   if (ioSessionRef() == NULL) RETURN_OUT_OF_MEMORY;
   if (AddNewSession(ioSessionRef).IsError(ret)) return ret;
   _networkIOSession = ioSessionRef;

   ScheduleSetBeaconData();

   LogTime(MUSCLE_LOG_INFO, "Starting up as peer [%s]\n", GetLocalPeerID().ToString()());

   // Make sure all of our databases are in their expected default states
   for (uint32 i=0; i<_databases.GetNumItems(); i++) _databases[i].ResetLocalDatabaseToDefaultState();

   return B_NO_ERROR;
}

void ZGPeerSession :: AboutToDetachFromServer()
{
   ShutdownChildSessions();  
   StorageReflectSession::AboutToDetachFromServer();
}

void ZGPeerSession :: EndSession()
{
   ShutdownChildSessions();  
   StorageReflectSession::EndSession();
}

void ZGPeerSession :: ShutdownChildSessions()
{
   if (_networkIOSession()) 
   {
      _networkIOSession()->EndSession();
      _networkIOSession.Reset();
   }
}

static MessageRef GetForwardedTextMessage(const String & cmd)
{
   MessageRef msg = GetMessageFromPool(PZG_PEER_COMMAND_USER_TEXT_MESSAGE);
   return ((msg())&&(msg()->AddString(PZG_PEER_NAME_TEXT, cmd) == B_NO_ERROR)) ? msg : MessageRef();
}

bool ZGPeerSession :: TextCommandReceived(const String & s)
{
   if (s.StartsWith("all peers "))
   {
      // Take the remainder of the Message and send it to everybody, to save on typing
      String cmd = s.Substring(10).Trim();
      MessageRef msg = GetForwardedTextMessage(cmd);
      if ((msg())&&(SendUnicastInternalMessageToAllPeers(msg) == B_NO_ERROR))
      {
         LogTime(MUSCLE_LOG_INFO, "Sending text command [%s] to all peers.\n", cmd());
         return TextCommandReceived(cmd); 
      }
      else
      {
         LogTime(MUSCLE_LOG_INFO, "Sending message [%s] to all peers failed!\n", cmd());
         return true;
      }
   }
   else if (s.StartsWith("senior peer")) 
   {
      // Take the remainder of the Message and send it to the senior peer
      String cmd = s.Substring(11).Trim();
      MessageRef msg = GetForwardedTextMessage(cmd);
      if ((msg())&&(SendUnicastInternalMessageToPeer(GetSeniorPeerID(), msg) == B_NO_ERROR))
      {
         LogTime(MUSCLE_LOG_INFO, "Sending text command [%s] to senior peer [%s].\n", cmd(), GetSeniorPeerID().ToString()());
         return true;
      }
      else
      {
         LogTime(MUSCLE_LOG_INFO, "Sending message [%s] to senior peer [%s] failed!\n", cmd(), GetSeniorPeerID().ToString()());
         return true;
      }
   }
   else if (s.StartsWith("print sessions")) 
   {
      PrintFactoriesInfo(); 
      PrintSessionsInfo();
   }
   else if (s.StartsWith("print network interfaces")) 
   {
      const PZGNetworkIOSession * nios = static_cast<const PZGNetworkIOSession *>(_networkIOSession());
      const PZGHeartbeatSettings * s = nios ? nios->GetPZGHeartbeatSettings()() : NULL;
      Queue<NetworkInterfaceInfo> niis; if (s) niis = s->GetNetworkInterfaceInfos();

      printf("Network interfaces currently in use by this ZG peer are:\n");
      for (uint32 i=0; i<niis.GetNumItems(); i++)
      {
         const NetworkInterfaceInfo & nii = niis[i];
         printf(UINT32_FORMAT_SPEC ". %s\n", i, nii.ToString()());
      }
   }
   else if ((s.StartsWith("print peers"))||(s == "pp")) 
   {
      if (_networkIOSession())
      {
         int i=0; 
         for (HashtableIterator<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > iter(static_cast<PZGNetworkIOSession*>(_networkIOSession())->GetMainThreadPeers()); iter.HasData(); iter++,i++)
            printf("Peer #%i: %s%s%s\n", i+1, iter.GetKey().ToString()(), (iter.GetKey()==GetLocalPeerID())?" <-- THIS PEER":"", (i==0)?" (SENIOR)":"");
      }
      else printf("Can't print peers list, network I/O session is missing!\n");
   }
   else if (s == "die")
   {
      LogTime(MUSCLE_LOG_INFO, "Requesting controlled process shutdown.\n");
      EndServer();
   }
   else if ((s == "enable time sync prints") ||(s == "etsp")) SetEnableTimeSynchronizationDebugging(true);
   else if ((s == "disable time sync prints")||(s == "dtsp")) SetEnableTimeSynchronizationDebugging(false);

   else return ParseGenericTextCommand(s);

   return true;
}


void ZGPeerSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   _onlinePeers.Put(peerID, peerInfo);

   if (_iAmFullyAttached == false)
   {
      _iAmFullyAttached = true;
      for (uint32 i=0; i<_databases.GetNumItems(); i++) _databases[i].ScheduleLogContentsRescan();
   }
}

void ZGPeerSession :: PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & /*peerInfo*/)
{
   _onlinePeers.Remove(peerID);
}

void ZGPeerSession :: SeniorPeerChanged(const ZGPeerID & oldSeniorPeerID, const ZGPeerID & newSeniorPeerID)
{
   if (newSeniorPeerID.IsValid()) 
   {
      if (oldSeniorPeerID.IsValid()) LogTime(MUSCLE_LOG_INFO, "Senior peer has changed from [%s] to [%s]\n", oldSeniorPeerID.ToString()(), newSeniorPeerID.ToString()());
                                else LogTime(MUSCLE_LOG_INFO, "Senior peer has been set to [%s]\n", newSeniorPeerID.ToString()());
   }
   else LogTime(MUSCLE_LOG_ERROR, "There is no longer any senior peer!\n");

   const bool iWasSeniorPeer = IAmTheSeniorPeer();
   _seniorPeerID = newSeniorPeerID;
   const bool iAmSeniorPeer = IAmTheSeniorPeer();

   if (iWasSeniorPeer != iAmSeniorPeer) 
   {
      LogTime(MUSCLE_LOG_INFO, "I am %s the senior peer of %s system [%s]!\n", IAmTheSeniorPeer()?"now":"no longer", GetPeerSettings().GetSignature()(), GetPeerSettings().GetSystemName()());
      LocalSeniorPeerStatusChanged();
      ScheduleSetBeaconData();
   }
}

bool ZGPeerSession :: IAmTheSeniorPeer() const
{
   const PZGNetworkIOSession * nios = static_cast<const PZGNetworkIOSession *>(_networkIOSession());
   if (nios)
   {
      const PZGHeartbeatSettings * s = nios->GetPZGHeartbeatSettings()();
      if (s) return (_seniorPeerID == s->GetLocalPeerID());
   }
   return false;
}

void ZGPeerSession :: LocalSeniorPeerStatusChanged()
{
   // empty
}

void ZGPeerSession :: PrivateMessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg)
{
   switch(msg()->what)
   {
      case PZG_PEER_COMMAND_RESET_SENIOR_DATABASE:
      case PZG_PEER_COMMAND_REPLACE_SENIOR_DATABASE:
      case PZG_PEER_COMMAND_UPDATE_SENIOR_DATABASE:
      case PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE:
         (void) HandleDatabaseUpdateRequest(fromPeerID, msg, (msg()->what != PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE));
      break;

      case PZG_PEER_COMMAND_USER_MESSAGE:
      {
         MessageRef userMsg = msg()->GetMessage(PZG_PEER_NAME_USER_MESSAGE);
         if (userMsg()) MessageReceivedFromPeer(fromPeerID, userMsg);
                   else LogTime(MUSCLE_LOG_ERROR, "ZGPeerSession::MessageReceivedFromPeer:  No user message inside wrapper from [%s]\n", fromPeerID.ToString()());
      }
      break;

      case PZG_PEER_COMMAND_USER_TEXT_MESSAGE:
      {
         const String * textStr = msg()->GetStringPointer(PZG_PEER_NAME_TEXT);
         if (textStr)
         {
            LogTime(MUSCLE_LOG_INFO, "Executing text command [%s] sent from peer [%s]\n", textStr->Cstr(), fromPeerID.ToString()());
            (void) TextCommandReceived(*textStr);
         }
      }
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "ZGPeerSession::PrivateMessageReceivedFromPeer:  Received unknown Message from [%s]:\n", fromPeerID.ToString()());
         msg()->PrintToStream();
      break;
   }
}

status_t ZGPeerSession :: HandleDatabaseUpdateRequest(const ZGPeerID & fromPeerID, const MessageRef & msg, bool isMessageMeantForSeniorPeer)
{
   const bool iAmSenior = IAmTheSeniorPeer();
   if (isMessageMeantForSeniorPeer != iAmSenior)
   {
      LogTime(MUSCLE_LOG_ERROR, "HandleDatabaseUpdateRequest:  Message " UINT32_FORMAT_SPEC " from peer [%s] was intended for %s peer, but I am %s\n", msg()->what, fromPeerID.ToString()(), isMessageMeantForSeniorPeer?"the senior":"a junior", iAmSenior?"the senior peer":"a junior peer");
      return B_BAD_DATA;
   }
   if ((isMessageMeantForSeniorPeer == false)&&(fromPeerID != GetSeniorPeerID()))
   {
      if (_iAmFullyAttached) LogTime(MUSCLE_LOG_ERROR, "HandleDatabaseUpdateRequest:  Message " UINT32_FORMAT_SPEC " was received from [%s], but the senior peer is [%s]\n", msg()->what, fromPeerID.ToString()(), GetSeniorPeerID().ToString()());
      return B_BAD_DATA;
   }

   uint32 whichDatabase;
   PZGDatabaseUpdateRef dbUp;
   if (msg()->what == PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE)
   {
      dbUp = GetPZGDatabaseUpdateFromPool();
      if ((dbUp() == NULL)||(msg()->FindFlat(PZG_PEER_NAME_DATABASE_UPDATE, *dbUp()) != B_NO_ERROR))
      {
         LogTime(MUSCLE_LOG_ERROR, "HandleDatabaseUpdateRequest:  Couldn't get PZGDatabaseUpdate from junior-update Message!\n");
         return B_BAD_DATA;
      }
      whichDatabase = dbUp()->GetDatabaseIndex();
   }
   else whichDatabase = msg()->GetInt32(PZG_PEER_NAME_DATABASE_ID);

   if (whichDatabase >= _databases.GetNumItems()) 
   {
      LogTime(MUSCLE_LOG_ERROR, "HandleDatabaseUpdateRequest:  Message " UINT32_FORMAT_SPEC " received from [%s] references database #" UINT32_FORMAT_SPEC ", but only " UINT32_FORMAT_SPEC " databases exist on this peer!\n", msg()->what, fromPeerID.ToString()(), whichDatabase, _databases.GetNumItems());
      return B_BAD_ARGUMENT;
   }

   return _databases[whichDatabase].HandleDatabaseUpdateRequest(fromPeerID, msg, dbUp);
}

status_t ZGPeerSession :: RequestResetDatabaseStateToDefault(uint32 whichDatabase)
{
   return SendRequestToSeniorPeer(whichDatabase, PZG_PEER_COMMAND_RESET_SENIOR_DATABASE, MessageRef());
}

status_t ZGPeerSession :: RequestReplaceDatabaseState(uint32 whichDatabase, const MessageRef & newDatabaseStateMsg)
{
   if (newDatabaseStateMsg() == NULL) return B_BAD_ARGUMENT;  // user's gotta specify something for us to base the new state on!
   return SendRequestToSeniorPeer(whichDatabase, PZG_PEER_COMMAND_REPLACE_SENIOR_DATABASE, newDatabaseStateMsg);
}

status_t ZGPeerSession :: RequestUpdateDatabaseState(uint32 whichDatabase, const MessageRef & databaseUpdateMsg)
{
   if (databaseUpdateMsg() == NULL) return B_BAD_ARGUMENT;  // user's gotta specify something for us to base the new state on!
   return SendRequestToSeniorPeer(whichDatabase, PZG_PEER_COMMAND_UPDATE_SENIOR_DATABASE, databaseUpdateMsg);
}

status_t ZGPeerSession :: SendRequestToSeniorPeer(uint32 whichDatabase, uint32 whatCode, const MessageRef & userMsg)
{
   if (whichDatabase >= _peerSettings.GetNumDatabases()) return B_BAD_ARGUMENT;  // invalid database index!
   if (_seniorPeerID.IsValid() == false) return B_BAD_OBJECT;  // can't send to senior peer if we don't know who he is!

   MessageRef sendMsg = GetMessageFromPool(whatCode);
   if (sendMsg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if ((sendMsg()->CAddInt32(PZG_PEER_NAME_DATABASE_ID,    whichDatabase).IsError(ret))||
       (sendMsg()->CAddMessage(PZG_PEER_NAME_USER_MESSAGE, userMsg).IsError(ret))) return ret;

   return SendUnicastInternalMessageToPeer(_seniorPeerID, sendMsg);
}

status_t ZGPeerSession :: RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok)
{
   PZGNetworkIOSession * nios = static_cast<PZGNetworkIOSession *>(_networkIOSession());
   if (nios == NULL) return B_LOGIC_ERROR;  // paranoia?
   
   return nios->RequestBackOrderFromSeniorPeer(ubok);
}

status_t ZGPeerSession :: SendDatabaseUpdateViaMulticast(const ConstPZGDatabaseUpdateRef & dbUp)
{
   PZGNetworkIOSession * nios = static_cast<PZGNetworkIOSession *>(_networkIOSession());
   if (nios == NULL) return B_LOGIC_ERROR;

   MessageRef wrapMsg = GetMessageFromPool(PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE);
   return ((wrapMsg())&&(wrapMsg()->AddFlat(PZG_PEER_NAME_DATABASE_UPDATE, FlatCountableRef(CastAwayConstFromRef(dbUp))) == B_NO_ERROR)) ? nios->SendMulticastMessageToAllPeers(wrapMsg) : B_ERROR;
}

static MessageRef WrapUserMessage(const MessageRef & userMsg)
{
   MessageRef wrapMsg = GetMessageFromPool(PZG_PEER_COMMAND_USER_MESSAGE);
   return ((wrapMsg())&&(wrapMsg()->AddMessage(PZG_PEER_NAME_USER_MESSAGE, userMsg) == B_NO_ERROR)) ? wrapMsg : MessageRef();
}

status_t ZGPeerSession :: SendMulticastUserMessageToAllPeers(const MessageRef & userMsg)
{
   return SendMulticastInternalMessageToAllPeers(WrapUserMessage(userMsg));
}

status_t ZGPeerSession :: SendMulticastInternalMessageToAllPeers(const MessageRef & internalMsg)
{
   if (internalMsg() == NULL) return B_BAD_ARGUMENT;

   PZGNetworkIOSession * nios = static_cast<PZGNetworkIOSession *>(_networkIOSession());
   return nios ? nios->SendMulticastMessageToAllPeers(internalMsg) : B_ERROR;
}

status_t ZGPeerSession :: SendUnicastUserMessageToAllPeers(const MessageRef & userMsg, bool sendToSelf)
{
   return SendUnicastInternalMessageToAllPeers(WrapUserMessage(userMsg), sendToSelf);
}

status_t ZGPeerSession :: SendUnicastInternalMessageToAllPeers(const MessageRef & internalMsg, bool sendToSelf)
{
   if (internalMsg() == NULL) return B_BAD_ARGUMENT;

   PZGNetworkIOSession * nios = static_cast<PZGNetworkIOSession *>(_networkIOSession());
   return nios ? nios->SendUnicastMessageToAllPeers(internalMsg, sendToSelf) : B_ERROR;
}

status_t ZGPeerSession :: SendUnicastUserMessageToPeer(const ZGPeerID & destinationPeerID, const MessageRef & userMsg)
{
   return SendUnicastInternalMessageToPeer(destinationPeerID, WrapUserMessage(userMsg));
}

status_t ZGPeerSession :: SendUnicastInternalMessageToPeer(const ZGPeerID & destinationPeerID, const MessageRef & internalMsg)
{
   if (internalMsg() == NULL) return B_BAD_ARGUMENT;

   PZGNetworkIOSession * nios = static_cast<PZGNetworkIOSession *>(_networkIOSession());
   return nios ? nios->SendUnicastMessageToPeer(destinationPeerID, internalMsg) : B_ERROR;
}

void ZGPeerSession :: PrintDatabaseStateInfo(int32 whichDatabase) const
{
   if ((whichDatabase > 0)&&((uint32)whichDatabase < _databases.GetNumItems())) _databases[whichDatabase].PrintDatabaseStateInfo();
   else
   {
      for (uint32 i=0; i<_databases.GetNumItems(); i++) _databases[i].PrintDatabaseStateInfo();
   }
}

void ZGPeerSession :: PrintDatabaseUpdateLog(int32 whichDatabase) const
{
   if ((whichDatabase > 0)&&((uint32)whichDatabase < _databases.GetNumItems())) _databases[whichDatabase].PrintDatabaseUpdateLog();
   else
   {
      for (uint32 i=0; i<_databases.GetNumItems(); i++) _databases[i].PrintDatabaseUpdateLog();
   }
}

void ZGPeerSession :: ScheduleSetBeaconData()
{
   if (_setBeaconDataPending == false)
   {
      _setBeaconDataPending = true;
      InvalidatePulseTime();
   }
}

uint64 ZGPeerSession :: GetPulseTime(const PulseArgs & args)
{
   if (_setBeaconDataPending) return 0;
   return StorageReflectSession::GetPulseTime(args);
}

ConstPZGBeaconDataRef ZGPeerSession :: GetNewSeniorBeaconData() const
{
   using namespace zg_private;

   PZGBeaconDataRef beaconDataRef = GetBeaconDataFromPool();
   if (beaconDataRef() == NULL) return ConstPZGBeaconDataRef();

   const uint32 numDBs = _databases.GetNumItems();
   Queue<PZGDatabaseStateInfo> & q = beaconDataRef()->GetDatabaseStateInfos();
   if (q.EnsureSize(numDBs) != B_NO_ERROR) return ConstPZGBeaconDataRef();

   for (uint32 i=0; i<numDBs; i++) (void) q.AddTail(_databases[i].GetDatabaseStateInfo());
   return AddConstToRef(beaconDataRef);
}

void ZGPeerSession :: Pulse(const PulseArgs & args)
{
   StorageReflectSession::Pulse(args);
   if (_setBeaconDataPending)
   {
      _setBeaconDataPending = false;

      // Force them to rescan their update logs now so that if they have data that needs to go out
      // it will go out *before* the new beacon-data.  That will prevent junior peers from unnecessarily
      // filing back-order requests simply because they got the new beacon-data before the new state-updates
      // that it refers to.
      for (uint32 i=0; i<_databases.GetNumItems(); i++) _databases[i].RescanUpdateLogIfNecessary();

      PZGNetworkIOSession * nios = static_cast<PZGNetworkIOSession *>(_networkIOSession());
      if (nios)
      {
         ConstPZGBeaconDataRef beaconData;
         if (IAmTheSeniorPeer()) beaconData = GetNewSeniorBeaconData();
         if (nios->SetBeaconData(beaconData) != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "ZGPeerSession:  Couldn't set beacon data!\n");
      }
   }
}

void ZGPeerSession :: BeaconDataChanged(const ConstPZGBeaconDataRef & beaconData)
{
   const uint32 numDBIs = beaconData() ? beaconData()->GetDatabaseStateInfos().GetNumItems() : 0;
   if (numDBIs == _databases.GetNumItems())
   {
      for (uint32 i=0; i<numDBIs; i++) _databases[i].SeniorDatabaseStateInfoChanged(beaconData()->GetDatabaseStateInfos()[i]);
   }
   else LogTime(MUSCLE_LOG_ERROR, "ZGPeerSession::DatabaseStateInfosChanged:  Wrong number of DBIs in update!  (Expected " UINT32_FORMAT_SPEC ", got " UINT32_FORMAT_SPEC ")\n", _databases.GetNumItems(),  numDBIs);
}


void ZGPeerSession :: BackOrderResultReceived(const PZGUpdateBackOrderKey & ubok, const ConstPZGDatabaseUpdateRef & optUpdateData)
{
   const uint32 whichDB = ubok.GetDatabaseIndex();
   if (whichDB < _databases.GetNumItems()) _databases[whichDB].BackOrderResultReceived(ubok, optUpdateData);
}

ConstPZGDatabaseUpdateRef ZGPeerSession :: GetDatabaseUpdateByID(uint32 whichDB, uint64 updateID) const
{
   if (whichDB >= _databases.GetNumItems())
   {
      LogTime(MUSCLE_LOG_ERROR, "ZGPeerSession::GetDatabaseUpdateByID:  Unknown database ID #" UINT32_FORMAT_SPEC "\n", whichDB);
      return ConstPZGDatabaseUpdateRef();
   }

   return _databases[whichDB].GetDatabaseUpdateByID(updateID);
}

int64 ZGPeerSession :: GetToNetworkTimeOffset() const
{
   const PZGNetworkIOSession * nios = static_cast<const PZGNetworkIOSession *>(_networkIOSession());
   return nios ? nios->GetToNetworkTimeOffset() : 0;
}

bool ZGPeerSession :: IsPeerOnline(const ZGPeerID & id) const
{
   const PZGNetworkIOSession * nios = static_cast<const PZGNetworkIOSession *>(_networkIOSession());
   return nios ? nios->IsPeerOnline(id) : false;
}

uint64 ZGPeerSession :: GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const
{
   const PZGNetworkIOSession * nios = static_cast<const PZGNetworkIOSession *>(_networkIOSession());
   return nios ? nios->GetEstimatedLatencyToPeer(peerID) : MUSCLE_TIME_NEVER;
}

String ZGPeerSession :: GetLocalDatabaseContentsAsString(uint32 /*whichDatabase*/) const
{
   return "(GetLocalDatabaseContentsAsString unimplemented)";
}

bool ZGPeerSession :: IsInSeniorDatabaseUpdateContext(uint32 whichDB) const
{
   return _databases[whichDB].IsInSeniorDatabaseUpdateContext();
}

bool ZGPeerSession :: IsInJuniorDatabaseUpdateContext(uint32 whichDB) const
{
   return _databases[whichDB].IsInJuniorDatabaseUpdateContext();
}

String PeerInfoToString(const ConstMessageRef & peerInfo)
{
   return zg_private::PeerInfoToString(peerInfo);
}

uint64 ZGPeerSession :: HandleDiscoveryPing(MessageRef & pingMsg, const IPAddressAndPort & /*pingSource*/)
{
   if (pingMsg()->what != PR_COMMAND_PING) return MUSCLE_TIME_NEVER;

   const ZGPeerSettings & s = GetPeerSettings();

   const PZGNetworkIOSession * nios = static_cast<const PZGNetworkIOSession *>(_networkIOSession());

   MessageRef pongMsg = GetMessageFromPool(PR_RESULT_PONG);
   if ((pongMsg() == NULL)
     || (pongMsg()->AddString( ZG_DISCOVERY_NAME_SYSTEMNAME, s.GetSystemName()).IsError())
     || (pongMsg()->AddString( ZG_DISCOVERY_NAME_SIGNATURE,  s.GetSignature()).IsError())
     || (pongMsg()->CAddInt16( ZG_DISCOVERY_NAME_TIMESYNCPORT, nios?nios->GetTimeSyncUDPPort():0).IsError())
     || (pongMsg()->AddFlat(   ZG_DISCOVERY_NAME_PEERID,     GetLocalPeerID()).IsError())) return MUSCLE_TIME_NEVER;

   if ((pingMsg()->HasName(ZG_DISCOVERY_NAME_TAG))&&(pingMsg()->ShareName(ZG_DISCOVERY_NAME_TAG, *pongMsg()).IsError())) return MUSCLE_TIME_NEVER;

   const Message * peerAttribs = s.GetPeerAttributes()();
   if (peerAttribs) for (MessageFieldNameIterator fnIter(*peerAttribs); fnIter.HasData(); fnIter++) peerAttribs->ShareName(fnIter.GetFieldName(), *pongMsg());

   // Do any client-specified filtering vs the pong-messages we're about to send back
   MessageRef qfMsg = pingMsg()->GetMessage(ZG_DISCOVERY_NAME_FILTER);
   if (qfMsg()) 
   {
      QueryFilterRef qfRef = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg());
      if ((qfRef())&&(qfRef()->Matches(pongMsg, NULL) == false)) return MUSCLE_TIME_NEVER;  // Nope!  We're not his type
   }

   pingMsg = pongMsg;
   return 0;
}

uint64 ZGPeerSession :: GetCurrentDatabaseStateID(uint32 whichDB) const
{
   return (whichDB < _databases.GetNumItems()) ? _databases[whichDB].GetCurrentDatabaseStateID() : 0;
}

bool ZGPeerSession :: UpdateLogContainsUpdate(uint32 whichDB, uint64 transactionID) const
{
   return (whichDB < _databases.GetNumItems()) ? _databases[whichDB].UpdateLogContainsUpdate(transactionID) : 0;
}

ConstMessageRef ZGPeerSession :: GetUpdatePayload(uint32 whichDB, uint64 transactionID) const
{
   return (whichDB < _databases.GetNumItems()) ? _databases[whichDB].GetDatabaseUpdatePayloadByID(transactionID) : ConstMessageRef();
}

};  // end namespace zg

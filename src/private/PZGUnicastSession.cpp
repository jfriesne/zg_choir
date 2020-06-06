#include "dataio/UDPSocketDataIO.h"
#include "util/NetworkUtilityFunctions.h"

#include "zg/ZGConstants.h"
#include "zg/private/PZGConstants.h"
#include "zg/private/PZGNetworkIOSession.h"
#include "zg/private/PZGUnicastSession.h"

namespace zg_private
{

enum {
   PZG_UNICAST_COMMAND_ANNOUNCE_UNICAST_PEER_ID = 1970170211,   // 'unic' 
   PZG_UNICAST_COMMAND_REQUEST_BACK_ORDER,
   PZG_UNICAST_COMMAND_REPLY_BACK_ORDER
};

static const String PZG_UNICAST_NAME_PEER_ID = "pid";

PZGUnicastSession :: PZGUnicastSession(PZGNetworkIOSession * master, const ZGPeerID & remotePeerID) 
   : _remotePeerID(remotePeerID)
   , _master(master)
{
   // empty
}

PZGUnicastSession :: ~PZGUnicastSession()
{
   // empty
}

status_t PZGUnicastSession :: AttachedToServer()
{
   status_t ret;
   if (AbstractReflectSession::AttachedToServer().IsError(ret)) return ret;

   RegisterMyself();

   // Do this now, so that it will definitely be the first Message we send after we are connected
   if (_remotePeerID.IsValid())
   {
      MessageRef msg = GetMessageFromPool(PZG_UNICAST_COMMAND_ANNOUNCE_UNICAST_PEER_ID);
      if ((msg())&&(msg()->AddFlat(PZG_UNICAST_NAME_PEER_ID, _master->GetPZGHeartbeatSettings()()->GetLocalPeerID()) == B_NO_ERROR)) (void) AddOutgoingMessage(msg);
   }
   return B_NO_ERROR;
}

void PZGUnicastSession :: AboutToDetachFromServer()
{
   UnregisterMyself(true);
   AbstractReflectSession::AboutToDetachFromServer();
}   

void PZGUnicastSession :: EndSession()
{
   UnregisterMyself(true);
   AbstractReflectSession::EndSession();
} 

void PZGUnicastSession :: MessageReceivedFromGateway(const MessageRef & msg, void *) 
{
   switch(msg()->what)
   {
      case PZG_UNICAST_COMMAND_ANNOUNCE_UNICAST_PEER_ID:
      {
         UnregisterMyself(false);  // unregister from our old ID (actually from our old lack-of-ID)

         _backorders.Clear();
         if (msg()->FindFlat(PZG_UNICAST_NAME_PEER_ID, _remotePeerID) == B_NO_ERROR) 
         {
            RegisterMyself(); // re-register under our new ID, now that we know what it is
         }
         else LogTime(MUSCLE_LOG_ERROR, "PZGUnicastSession:  Couldn't find peer ID in PZG_UNICAST_NAME_PEER_ID message!\n");
      }
      break;

      case PZG_UNICAST_COMMAND_REQUEST_BACK_ORDER:
      {
         const uint32 whichDB  = msg()->GetInt32(PZG_PEER_NAME_DATABASE_ID);
         const uint64 updateID = msg()->GetInt64(PZG_PEER_NAME_DATABASE_UPDATE_ID);
         ConstPZGDatabaseUpdateRef dbUp = _master->GetDatabaseUpdateByID(whichDB, updateID);
         if ((dbUp() == NULL)||(msg()->AddFlat(PZG_PEER_NAME_DATABASE_UPDATE, *dbUp()) != B_NO_ERROR)) LogTime(MUSCLE_LOG_ERROR, "PZGUnicastSession::MessageReceivedFromGateway()():  Database #" UINT32_FORMAT_SPEC " doesn't have requested back-order " UINT64_FORMAT_SPEC " to send back to junior peer [%s]\n", whichDB, updateID, _remotePeerID.ToString()());

         msg()->what = PZG_UNICAST_COMMAND_REPLY_BACK_ORDER;  // we're going to send this Message right back as our reply
         if (AddOutgoingMessage(msg) != B_NO_ERROR) 
         {
            LogTime(MUSCLE_LOG_ERROR, "Unable to send back-order reply back to junior peer [%s]\n", _remotePeerID.ToString()());
            EndSession();  // semi-paranoia:  might as well terminate the connection, so that at least the remote peer won't wait forever for his reply
         }
      }
      break;

      case PZG_UNICAST_COMMAND_REPLY_BACK_ORDER:
      {
         PZGDatabaseUpdateRef dbUp = GetPZGDatabaseUpdateFromPool();
         if ((dbUp())&&(msg()->FindFlat(PZG_PEER_NAME_DATABASE_UPDATE, *dbUp()) != B_NO_ERROR)) dbUp.Reset();

         const uint32 whichDB  = msg()->GetInt32(PZG_PEER_NAME_DATABASE_ID);
         const uint64 updateID = msg()->GetInt64(PZG_PEER_NAME_DATABASE_UPDATE_ID);
         const PZGUpdateBackOrderKey ubok(_remotePeerID, whichDB, updateID);
         if (_backorders.Remove(ubok) == B_NO_ERROR)
         {
            _master->BackOrderResultReceived(ubok, dbUp);
         }
         else LogTime(MUSCLE_LOG_WARNING, "PZGUnicastSession:  Got a back-order reply that I don't remember asking for (db=" UINT32_FORMAT_SPEC " updateID=" UINT64_FORMAT_SPEC ")\n", whichDB, updateID);
      }
      break;

      default:
         _master->UnicastMessageReceivedFromPeer(_remotePeerID, msg);
      break;
   }
}

void PZGUnicastSession :: RegisterMyself()
{
   if (_master) _master->RegisterUnicastSession(this);  // this will register us with our Peer ID, if we're an outgoing session, or as an anymous session if it's incoming TCP
}

void PZGUnicastSession :: UnregisterMyself(bool forGood)
{
   if (_master) 
   {
      // If we have any back-orders outstanding, make sure the master knows they aren't going to happen
      for (HashtableIterator<PZGUpdateBackOrderKey, Void> iter(_backorders); iter.HasData(); iter++) _master->BackOrderResultReceived(iter.GetKey(), ConstPZGDatabaseUpdateRef());
   }
   _backorders.Clear();

   if (_master) 
   {
      _master->UnregisterUnicastSession(this);
      if (forGood) _master = NULL;
   }
}

status_t PZGUnicastSession :: RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok)
{
   if (_backorders.ContainsKey(ubok)) return B_NO_ERROR;  // semi-paranoia:  if it's already on back-order, no need to ask again

   status_t ret;
   if (_backorders.PutWithDefault(ubok).IsOK(ret))
   {
      MessageRef msg = GetMessageFromPool(PZG_UNICAST_COMMAND_REQUEST_BACK_ORDER);
      if (msg() == NULL) RETURN_OUT_OF_MEMORY;
      if ((msg()->CAddInt32(PZG_PEER_NAME_DATABASE_ID, ubok.GetDatabaseIndex()).IsOK(ret))&&(msg()->AddInt64(PZG_PEER_NAME_DATABASE_UPDATE_ID, ubok.GetDatabaseUpdateID()).IsOK(ret))&&(AddOutgoingMessage(msg).IsOK(ret))) return B_NO_ERROR;
      (void) _backorders.Remove(ubok);  // roll back!
   }
   return ret;
}

};  // end namespace zg_private

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
   MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());

   RegisterMyself();

   // Do this now, so that it will definitely be the first Message we send after we are connected
   if (_remotePeerID.IsValid())
   {
      MessageRef msg = GetMessageFromPool(PZG_UNICAST_COMMAND_ANNOUNCE_UNICAST_PEER_ID);
      if ((msg())&&(msg()->AddFlat(PZG_UNICAST_NAME_PEER_ID, _master->GetPZGHeartbeatSettings()()->GetLocalPeerID()).IsOK())) (void) AddOutgoingMessage(msg);
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
         if (msg()->FindFlat(PZG_UNICAST_NAME_PEER_ID, _remotePeerID).IsOK())
         {
            RegisterMyself(); // re-register under our new ID, now that we know what it is
         }
         else LogTime(MUSCLE_LOG_ERROR, "PZGUnicastSession:  Couldn't find peer ID in PZG_UNICAST_NAME_PEER_ID message!\n");
      }
      break;

      case PZG_UNICAST_COMMAND_REQUEST_BACK_ORDER:
      {
         status_t ret;
         PZGUpdateBackOrderKey ubok;
         if (msg()->FindFlat(PZG_PEER_NAME_BACK_ORDER, ubok).IsError(ret))
         {
            LogTime(MUSCLE_LOG_ERROR, "PZG_UNICAST_COMMAND_REQUEST_BACK_ORDER:  Couldn't get PZGUpdateBackOrderKey from Message!  [%s]\n", ret());
            return;
         }

         const uint32 whichDB  = ubok.GetDatabaseIndex();
         const uint64 updateID = ubok.GetDatabaseUpdateID();
         if ((updateID == DATABASE_UPDATE_ID_FULL_UPDATE)&&(msg()->HasName(PZG_PEER_NAME_CHECKSUM_MISMATCH))) _master->VerifyOrFixLocalDatabaseChecksum(whichDB);  // so we can recover if the checksum has gone wrong

         ConstPZGDatabaseUpdateRef dbUp = _master->GetDatabaseUpdateByID(whichDB, updateID);
         if ((dbUp() == NULL)||(msg()->AddFlat(PZG_PEER_NAME_DATABASE_UPDATE, *dbUp()).IsError())) LogTime(MUSCLE_LOG_ERROR, "PZGUnicastSession::MessageReceivedFromGateway()():  Database #" UINT32_FORMAT_SPEC " doesn't have requested back-order " UINT64_FORMAT_SPEC " to send back to junior peer [%s]\n", whichDB, updateID, _remotePeerID.ToString()());

         msg()->what = PZG_UNICAST_COMMAND_REPLY_BACK_ORDER;  // we're going to send this Message right back as our reply
         if (AddOutgoingMessage(msg).IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "Unable to send back-order reply back to junior peer [%s]\n", _remotePeerID.ToString()());
            EndSession();  // semi-paranoia:  might as well terminate the connection, so that at least the remote peer won't wait forever for his reply
         }
      }
      break;

      case PZG_UNICAST_COMMAND_REPLY_BACK_ORDER:
      {
         PZGDatabaseUpdateRef dbUp = GetPZGDatabaseUpdateFromPool();
         if ((dbUp())&&(msg()->FindFlat(PZG_PEER_NAME_DATABASE_UPDATE, *dbUp()).IsError())) dbUp.Reset();

         status_t ret;
         PZGUpdateBackOrderKey ubok;
         if (msg()->FindFlat(PZG_PEER_NAME_BACK_ORDER, ubok).IsError(ret))
         {
            LogTime(MUSCLE_LOG_ERROR, "PZG_UNICAST_COMMAND_REPLY_BACK_ORDER:  Couldn't get PZGUpdateBackOrderKey from Message!  [%s]\n", ret());
            return;
         }

         if (_backorders.Remove(ubok).IsOK())
         {
            _master->BackOrderResultReceived(ubok, dbUp);
         }
         else LogTime(MUSCLE_LOG_WARNING, "PZGUnicastSession:  Got a back-order reply that I don't remember asking for (%s)\n", ubok.ToString()());
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

status_t PZGUnicastSession :: RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok, bool dueToChecksumError)
{
   if (_backorders.ContainsKey(ubok)) return B_NO_ERROR;  // semi-paranoia:  if it's already on back-order, no need to ask again

   MessageRef msg = GetMessageFromPool(PZG_UNICAST_COMMAND_REQUEST_BACK_ORDER);
   MRETURN_OOM_ON_NULL(msg());
   MRETURN_ON_ERROR(msg()->AddFlat(PZG_PEER_NAME_BACK_ORDER,         ubok));
   MRETURN_ON_ERROR(msg()->CAddBool(PZG_PEER_NAME_CHECKSUM_MISMATCH, dueToChecksumError));
   MRETURN_ON_ERROR(AddOutgoingMessage(msg));
   return _backorders.PutWithDefault(ubok);
}

};  // end namespace zg_private

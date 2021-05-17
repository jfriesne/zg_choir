#include "zg/messagetree/server/ClientDataMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"

namespace zg 
{

// Command codes used internally by ClientDataMessageTreeDatabaseObject implementation
enum {
   CLIENTDATA_COMMAND_UPLOADLOCALDATA = 1667526004, // 'cdmt' -- send by senior peer to a junior peer when the senior peer wants the shared-local-data-cache for that junior peer to be refreshed
   CLIENTDATA_COMMAND_LOCALDATA,                    // reply back from the junior peer to the senior peer
};
static const String CLIENTDATA_NAME_PATH    = "pth";
static const String CLIENTDATA_NAME_PAYLOAD = "pay";

ClientDataMessageTreeDatabaseObject :: ClientDataMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : MessageTreeDatabaseObject(session, dbIndex, rootNodePath)
{
   // empty
}

status_t ClientDataMessageTreeDatabaseObject :: UploadNodeValue(const String & localPath, const MessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?
   
   status_t ret = MessageTreeDatabaseObject::UploadNodeValue(sharedPath, optPayload, flags, optBefore, optOpTag);
   if (ret.IsError()) return ret;

   // Also Update the node in our server-local MUSCLE database, so that we can retransmit it later if we need to
   return ssmts->SetDataNode(localPath, optPayload, ConvertTreeGatewayFlagsToSetDataNodeFlags(flags), optBefore.HasChars()?&optBefore:NULL);
}

status_t ClientDataMessageTreeDatabaseObject :: UploadNodeSubtree(const String & localPath, const MessageRef & valuesMsg, TreeGatewayFlags flags, const String & optOpTag)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::UploadNodeSubtree(sharedPath, valuesMsg, flags, optOpTag);
   if (ret.IsError()) return ret;

   // Upload the subtree in our local MUSCLE database also, so that we can retransmit it later if we need to
   SetDataNodeFlags sdnFlags;
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY)) sdnFlags.SetBit(SETDATANODE_FLAG_QUIET);
   return ssmts->RestoreNodeTreeFromMessage(*valuesMsg(), localPath, true, sdnFlags);
}

status_t ClientDataMessageTreeDatabaseObject :: RequestDeleteNodes(const String & localPath, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags, const String & optOpTag)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, optFilter, flags, optOpTag);
   if (ret.IsError()) return ret;

   return ssmts->RemoveDataNodes(localPath, optFilter, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
}

status_t ClientDataMessageTreeDatabaseObject :: RequestMoveIndexEntry(const String & localPath, const String & optBefore, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags, const String & optOpTag)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   const status_t ret = MessageTreeDatabaseObject::RequestMoveIndexEntry(sharedPath, optBefore, optFilter, flags, optOpTag);
   if (ret.IsError()) return ret;

   return ssmts->MoveIndexEntries(localPath, optBefore.HasChars()?&optBefore:NULL, optFilter);
}

String ClientDataMessageTreeDatabaseObject :: GetSharedPathFromLocalPath(const String & localPath, ServerSideMessageTreeSession * & retSessionNode) const
{
   retSessionNode = GetMessageTreeDatabasePeerSession()->GetActiveServerSideMessageTreeSession();
   return GetSharedPathFromLocalPathAux(localPath, retSessionNode);
}

String ClientDataMessageTreeDatabaseObject :: GetSharedPathFromLocalPathAux(const String & localPath, ServerSideMessageTreeSession * ssmts) const
{
   return ssmts ? (GetMessageTreeDatabasePeerSession()->GetLocalPeerID().ToString() + ssmts->GetSessionRootPath()).AppendWord(localPath, "/") : GetEmptyString();
}

void ClientDataMessageTreeDatabaseObject :: ServerSideMessageTreeSessionIsDetaching(ServerSideMessageTreeSession * ssmts)
{
   const String sharedPath = GetSharedPathFromLocalPathAux(GetEmptyString(), ssmts);
   if (sharedPath.HasChars())
   {
      GatewaySubscriberCommandBatchGuard<ITreeGatewaySubscriber> gcb(ssmts);

      // Request the deletion of any shared-nodes corresponding to the departing client's local data.
      (void) MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, ConstQueryFilterRef(), TreeGatewayFlags(), GetEmptyString());

      // We'll also send a request to delete the parent/IP-address node, but only if it no longer has any session nodes beneath
      // it.  That way the shared-database won't contain "orphan client IP addresses" with no sessions left in them.
      int32 lastSlash = sharedPath.LastIndexOf('/');
      if (lastSlash > 0)
      {
         const String ipPath = sharedPath.Substring(0, lastSlash);

         static const ChildCountQueryFilter _noKids(ChildCountQueryFilter::OP_EQUAL_TO, 0);
         (void) MessageTreeDatabaseObject::RequestDeleteNodes(ipPath, DummyConstQueryFilterRef(_noKids), TreeGatewayFlags(), GetEmptyString());

         // And if the grandparent/peer-ID node has also become empty, we can delete that too
         lastSlash = ipPath.LastIndexOf('/');
         if (lastSlash > 0) (void) MessageTreeDatabaseObject::RequestDeleteNodes(ipPath.Substring(0, lastSlash), DummyConstQueryFilterRef(_noKids), TreeGatewayFlags(), GetEmptyString());
      }
   }
}

static Message _uploadLocalDataRequestMsg(CLIENTDATA_COMMAND_UPLOADLOCALDATA);

void ClientDataMessageTreeDatabaseObject :: LocalSeniorPeerStatusChanged()
{
   MessageTreeDatabaseObject::LocalSeniorPeerStatusChanged();
   if (GetDatabasePeerSession()->IAmTheSeniorPeer())
   {
      // Delete any peerID-nodes for peer-IDs that are not currently on line
      String nodesToDelete;
      {
         const DataNode * dn = GetMessageTreeDatabasePeerSession()->GetDataNode(GetRootPathWithoutSlash());
         if (dn)
         {
            for (DataNodeRefIterator iter = dn->GetChildIterator(); iter.HasData(); iter++)
            {
               ZGPeerID pid; pid.FromString(*iter.GetKey());
               if (GetMessageTreeDatabasePeerSession()->IsPeerOnline(pid) == false) nodesToDelete = nodesToDelete.AppendWord(*iter.GetKey(),",");
            }
         }
      }

      if (nodesToDelete.HasChars()) 
      {
         LogTime(MUSCLE_LOG_INFO, "ClientDataMessageTreeDatabaseObject assuming senior-peer status, flushing nodes for offline peers [%s]\n", nodesToDelete());
         MessageTreeDatabaseObject::RequestDeleteNodes(nodesToDelete, ConstQueryFilterRef(), TreeGatewayFlags(), GetEmptyString());
      }

      // Tell all of the junior peers to resend their local data to us, in case it got corrupted during the confusion
      SendMessageToDatabaseObject(ZGPeerID(), DummyMessageRef(_uploadLocalDataRequestMsg));
   }
}

void ClientDataMessageTreeDatabaseObject :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   MessageTreeDatabaseObject::PeerHasComeOnline(peerID, peerInfo);
   if (GetDatabasePeerSession()->IAmTheSeniorPeer()) SendMessageToDatabaseObject(peerID, DummyMessageRef(_uploadLocalDataRequestMsg));
}

void ClientDataMessageTreeDatabaseObject :: PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   MessageTreeDatabaseObject::PeerHasGoneOffline(peerID, peerInfo);
   if (GetDatabasePeerSession()->IAmTheSeniorPeer()) (void) MessageTreeDatabaseObject::RequestDeleteNodes(peerID.ToString(), ConstQueryFilterRef(), TreeGatewayFlags(), GetEmptyString());
}

void ClientDataMessageTreeDatabaseObject :: MessageReceivedFromMessageTreeDatabaseObject(const MessageRef & msg, const ZGPeerID & sourcePeer, uint32 sourceDBIdx)
{
   switch(msg()->what)
   {
      case CLIENTDATA_COMMAND_UPLOADLOCALDATA:
      {
         LogTime(MUSCLE_LOG_INFO, "ClientDataMessageTreeDatabaseObject:  Re-uploading local client data to the ZG shared database.\n");

         MessageRef replyMsg;
         for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetMessageTreeDatabasePeerSession()->GetSessions()); iter.HasData(); iter++)
         {
            ServerSideMessageTreeSession * ssmts = dynamic_cast<ServerSideMessageTreeSession *>(iter.GetValue()());
            if (ssmts)
            { 
               MessageRef subtreeMsg = GetMessageFromPool();
               if (subtreeMsg())
               {
                  status_t ret;
                  if (ssmts->SaveNodeTreeToMessage(*subtreeMsg(), ssmts->GetSessionNode()(), GetEmptyString(), true).IsOK(ret))
                  {
                     // Note that the reason I'm sending a CLIENTDATA_COMMAND_LOCALDATA Message back to the senior peer's database instead
                     // of just calling MessageTreeDatabaseObject::UploadNodeSubtree() directly from here is only because I'm paranoid that
                     // in a chaotic situation where the old senior peer has just gone offline, the junior peers might not yet realize who
                     // the new senior peer is, and thus a simple call to UploadNodeSubtree() might end up being sent to the old senior peer
                     // who (being dead) will not act on it.  By sending the Message back explicitly to the (sourcePeer) who sent us the
                     // CLIENTDATA_COMMAND_UPLOADLOCALDATA Message, I avoid that potential "gotcha".  --jaf
                     const String sharedPath = GetSharedPathFromLocalPathAux(GetEmptyString(), ssmts);
                     if (replyMsg() == NULL) replyMsg = GetMessageFromPool(CLIENTDATA_COMMAND_LOCALDATA);
                     if (replyMsg())
                     {
                        const status_t ret = replyMsg()->AddString(CLIENTDATA_NAME_PATH, sharedPath) | replyMsg()->AddMessage(CLIENTDATA_NAME_PAYLOAD, subtreeMsg);
                        if (ret.IsError()) 
                        {
                           LogTime(MUSCLE_LOG_ERROR, "ClientDataMessageTreeDatabaseObject:  Unable to populate CLIENTDATA_COMMAND_LOCALDATA Message!  [%s]\n", ret());
                           return;
                        }
                     }
                     else MWARN_OUT_OF_MEMORY;
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "ClientDataMessageTreeDatabaseObject:  Unable to save client session [%s]'s local data-subtree to a Message! [%s]\n", ssmts->GetSessionRootPath()(), ret());
               }
               else MWARN_OUT_OF_MEMORY;
            }
         }

         status_t ret;
         if ((replyMsg())&&(SendMessageToDatabaseObject(sourcePeer, replyMsg, sourceDBIdx).IsError(ret))) LogTime(MUSCLE_LOG_ERROR, "ClientDataMessageTreeDatabaseObject:  Unable to send CLIENTDATA_COMMAND_LOCALDATA back to source peer!  [%s]\n", ret());
      }
      break;

      case CLIENTDATA_COMMAND_LOCALDATA:
      {
         if (GetDatabasePeerSession()->IAmTheSeniorPeer())
         {
            const String * nextPath;
            MessageRef nextPayload;
            for (int32 i=0; ((msg()->FindString(CLIENTDATA_NAME_PATH, i, &nextPath).IsOK())&&(msg()->FindMessage(CLIENTDATA_NAME_PAYLOAD, i, nextPayload).IsOK())); i++)
            {
               status_t ret;
               if (MessageTreeDatabaseObject::UploadNodeSubtree(*nextPath, nextPayload, TreeGatewayFlags(), GetEmptyString()).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "ClientDataMessageTreeDatabaseObject:  shared-data UploadNodeSubtree(%s) failed.  [%s]\n", nextPath->Cstr(), ret());
            }
         }
      }
      break;

      default:
         MessageTreeDatabaseObject::MessageReceivedFromMessageTreeDatabaseObject(msg, sourcePeer, sourceDBIdx);
      break;
   }
}

}; // end namespace zg

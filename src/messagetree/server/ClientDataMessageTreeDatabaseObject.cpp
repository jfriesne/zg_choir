#include "zg/messagetree/server/ClientDataMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"

namespace zg 
{

// Command codes used internally by ClientDataMessageTreeDatabaseObject implementation
enum {
   CLIENTDATA_COMMAND_UPLOADLOCALDATA = 1667526004, // 'cdmt' 
};


ClientDataMessageTreeDatabaseObject :: ClientDataMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : MessageTreeDatabaseObject(session, dbIndex, rootNodePath)
{
   // empty
}

status_t ClientDataMessageTreeDatabaseObject :: UploadNodeValue(const String & localPath, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?
   
   status_t ret = MessageTreeDatabaseObject::UploadNodeValue(sharedPath, optPayload, flags, optBefore);
   if (ret.IsError()) return ret;

   // Also Update the node in our server-local MUSCLE database, so that we can retransmit it later if we need to
   return ssmts->SetDataNode(localPath, optPayload, true, true, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY), flags.IsBitSet(TREE_GATEWAY_FLAG_INDEXED), optBefore);
}

status_t ClientDataMessageTreeDatabaseObject :: UploadNodeSubtree(const String & localPath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::UploadNodeSubtree(sharedPath, valuesMsg, flags);
   if (ret.IsError()) return ret;

   // Upload the subtree in our local MUSCLE database also, so that we can retransmit it later if we need to
   return ssmts->RestoreNodeTreeFromMessage(*valuesMsg(), localPath, true, false, MUSCLE_NO_LIMIT, NULL, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
}

status_t ClientDataMessageTreeDatabaseObject :: RequestDeleteNodes(const String & localPath, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, optFilter, flags);
   if (ret.IsError()) return ret;

   return ssmts->RemoveDataNodes(localPath, optFilter, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
}

status_t ClientDataMessageTreeDatabaseObject :: RequestMoveIndexEntry(const String & localPath, const String * optBefore, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags)
{
   ServerSideMessageTreeSession * ssmts = NULL;
   const String sharedPath = GetSharedPathFromLocalPath(localPath, ssmts);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   const status_t ret = MessageTreeDatabaseObject::RequestMoveIndexEntry(sharedPath, optBefore, optFilter, flags);
   if (ret.IsError()) return ret;

   return ssmts->MoveIndexEntries(localPath, optBefore, optFilter);;
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
      (void) MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, ConstQueryFilterRef(), TreeGatewayFlags());

      // We'll also send a request to delete the parent/IP-address node, but only if it no longer has any session nodes beneath
      // it.  That way the shared-database won't contain "orphan client IP addresses" with no sessions left in them.
      int32 lastSlash = sharedPath.LastIndexOf('/');
      if (lastSlash > 0)
      {
         const String ipPath = sharedPath.Substring(0, lastSlash);

         static const ChildCountQueryFilter _noKids(ChildCountQueryFilter::OP_EQUAL_TO, 0);
         (void) MessageTreeDatabaseObject::RequestDeleteNodes(ipPath, ConstQueryFilterRef(&_noKids, false), TreeGatewayFlags());

         // And if the grandparent/peer-ID node has also become empty, we can delete that too
         lastSlash = ipPath.LastIndexOf('/');
         if (lastSlash > 0) (void) MessageTreeDatabaseObject::RequestDeleteNodes(ipPath.Substring(0, lastSlash), ConstQueryFilterRef(&_noKids, false), TreeGatewayFlags());
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
         MessageTreeDatabaseObject::RequestDeleteNodes(nodesToDelete, ConstQueryFilterRef(), TreeGatewayFlags());
      }

      // Tell all of the junior peers to resend their local data to us, in case it got corrupted during the confusion
      SendMessageToDatabaseObject(ZGPeerID(), MessageRef(&_uploadLocalDataRequestMsg, false));
   }
}

void ClientDataMessageTreeDatabaseObject :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   MessageTreeDatabaseObject::PeerHasComeOnline(peerID, peerInfo);
   if (GetDatabasePeerSession()->IAmTheSeniorPeer()) SendMessageToDatabaseObject(peerID, MessageRef(&_uploadLocalDataRequestMsg, false));
}

void ClientDataMessageTreeDatabaseObject :: PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   MessageTreeDatabaseObject::PeerHasGoneOffline(peerID, peerInfo);
   if (GetDatabasePeerSession()->IAmTheSeniorPeer()) (void) MessageTreeDatabaseObject::RequestDeleteNodes(peerID.ToString(), ConstQueryFilterRef(), TreeGatewayFlags());
}

void ClientDataMessageTreeDatabaseObject :: MessageReceivedFromMessageTreeDatabaseObject(const MessageRef & msg, const ZGPeerID & sourcePeer, uint32 sourceDBIdx)
{
   switch(msg()->what)
   {
      case CLIENTDATA_COMMAND_UPLOADLOCALDATA:
      {
         LogTime(MUSCLE_LOG_INFO, "ClientDataMessageTreeDatabaseObject:  Re-uploading local client data to the ZG shared database.\n");
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
                     const String sharedPath = GetSharedPathFromLocalPathAux(GetEmptyString(), ssmts);
                     if (MessageTreeDatabaseObject::UploadNodeSubtree(sharedPath, subtreeMsg, TreeGatewayFlags()).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "ClientDataMessageTreeDatabaseObject:  UploadNodeSubtree(%s) failed for client session [%s] [%s]\n", sharedPath(), ssmts->GetSessionRootPath()(), ret());

                  }
                  else LogTime(MUSCLE_LOG_ERROR, "ClientDataMessageTreeDatabaseObject:  Unable to save client session [%s]'s local data-subtree to a Message! [%s]\n", ssmts->GetSessionRootPath()(), ret());
               }
               else WARN_OUT_OF_MEMORY;
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

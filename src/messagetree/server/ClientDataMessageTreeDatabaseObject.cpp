#include "zg/messagetree/server/ClientDataMessageTreeDatabaseObject.h"
#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/ServerSideMessageTreeSession.h"

namespace zg 
{

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

void ClientDataMessageTreeDatabaseObject :: ServerSideMessageTreeSessionIsDetaching(ServerSideMessageTreeSession * clientSession)
{
   const String sharedPath = GetSharedPathFromLocalPathAux(GetEmptyString(), clientSession);
   if (sharedPath.HasChars())
   {
      GatewaySubscriberCommandBatchGuard<ITreeGatewaySubscriber> gcb(clientSession);

      (void) MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, ConstQueryFilterRef(), TreeGatewayFlags());

      // We'll also send a request to delete the parent/IP-address node, but only if it no longer has any session nodes beneath
      // it.  That way the database won't contain "orphan client IP addresses" with no sessions left in them.
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

}; // end namespace zg

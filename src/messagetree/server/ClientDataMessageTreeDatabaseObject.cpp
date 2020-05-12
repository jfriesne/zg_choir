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

   // Update the node in our local MUSCLE database also, so that we can retransmit it later if we need to
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
   if (sharedPath.HasChars()) (void) MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, ConstQueryFilterRef(), TreeGatewayFlags());
}

}; // end namespace zg

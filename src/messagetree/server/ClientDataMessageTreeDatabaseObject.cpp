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

status_t ClientDataMessageTreeDatabaseObject :: UploadNodeValue(const String & localPath, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
   const String sharedPath = GetSharedPathFromLocalPath(localPath);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?
   
   status_t ret = MessageTreeDatabaseObject::UploadNodeValue(sharedPath, optPayload, flags, optBefore);
printf("CCCA upload node [%s]->[%s] %p ret=%s\n", localPath(), sharedPath(), optPayload(), ret());
PrintStackTrace();

   if (ret.IsError()) return ret;

   return B_NO_ERROR;
}

status_t ClientDataMessageTreeDatabaseObject :: UploadNodeSubtree(const String & localPath, const MessageRef & valuesMsg, TreeGatewayFlags flags)
{
   const String sharedPath = GetSharedPathFromLocalPath(localPath);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::UploadNodeSubtree(sharedPath, valuesMsg, flags);
printf("CCCB upload subtree [%s] %p ret=%s\n", sharedPath(), valuesMsg(), ret());
   if (ret.IsError()) return ret;

   return B_NO_ERROR;
}

status_t ClientDataMessageTreeDatabaseObject :: RequestDeleteNodes(const String & localPath, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags)
{
   const String sharedPath = GetSharedPathFromLocalPath(localPath);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::RequestDeleteNodes(sharedPath, optFilter, flags);
printf("CCCC delete nodes [%s] ret=%s\n", sharedPath(), ret());
   if (ret.IsError()) return ret;

   return B_NO_ERROR;
}

status_t ClientDataMessageTreeDatabaseObject :: RequestMoveIndexEntry(const String & localPath, const char * optBefore, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags)
{
   const String sharedPath = GetSharedPathFromLocalPath(localPath);
   if (sharedPath.IsEmpty()) return B_BAD_OBJECT;  // Perhaps we weren't called from within anyone's MessageReceivedFromGateway() method?

   status_t ret = MessageTreeDatabaseObject::RequestMoveIndexEntry(sharedPath, optBefore, optFilter, flags);
printf("CCCD move index entries [%s] ret=%s\n", sharedPath(), ret());
   if (ret.IsError()) return ret;

   return B_NO_ERROR;
}

String ClientDataMessageTreeDatabaseObject :: GetSharedPathFromLocalPath(const String & localPath) const
{
   ServerSideMessageTreeSession * ssmts = GetMessageTreeDatabasePeerSession()->GetActiveServerSideMessageTreeSession();
   return ssmts ? (GetMessageTreeDatabasePeerSession()->GetLocalPeerID().ToString() + ssmts->GetSessionRootPath() + "/" + localPath) : GetEmptyString();
}

}; // end namespace zg

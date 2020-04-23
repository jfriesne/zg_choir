#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"

namespace zg 
{

enum {
   MTDO_SENIOR_COMMAND_BATCH = 1836344431, // 'mtdo' 
   MTDO_SENIOR_COMMAND_UPLOADNODEVALUE,
   MTDO_SENIOR_COMMAND_REQUESTDELETENODES,
};

static const String MTDO_NAME_PATH       = "pth";
static const String MTDO_NAME_PAYLOAD    = "pay";
static const String MTDO_NAME_FLAGS      = "flg";
static const String MTDO_NAME_BEFORE     = "be4";
static const String MTDO_NAME_SUBMESSAGE = "sub";
static const String MTDO_NAME_FILTER     = "fil";

MessageTreeDatabaseObject :: MessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath) 
   : IDatabaseObject(session, dbIndex)
   , _rootNodePath(rootNodePath.WithSuffix("/"))
   , _checksum(0)
{
   // empty
}

void MessageTreeDatabaseObject :: SetToDefaultState()
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh) (void) zsh->RemoveDataNodes(_rootNodePath);
}

status_t MessageTreeDatabaseObject :: SetFromArchive(const ConstMessageRef & archive)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   return zsh->RestoreNodeTreeFromMessage(*archive(), _rootNodePath, true, true);
}

status_t MessageTreeDatabaseObject :: SaveToArchive(const MessageRef & archive) const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   const DataNode * rootNode = zsh->GetDataNode(_rootNodePath);
   return rootNode ? zsh->SaveNodeTreeToMessage(*archive(), rootNode, GetEmptyString(), true) : B_NO_ERROR;
}

uint32 MessageTreeDatabaseObject :: CalculateChecksum() const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return 0;

   const DataNode * rootNode = zsh->GetDataNode(_rootNodePath);
   return rootNode ? rootNode->CalculateChecksum() : 0;
}

ConstMessageRef MessageTreeDatabaseObject :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   GatewaySubscriberCommandBatchGuard<ITreeGateway> batchGuard(GetMessageTreeDatabasePeerSession());  // so that MessageTreeDatabasePeerSession::CommandBatchEnds() will call PushSubscriptionMessages() when we're done

   // Setup stuff here

   const status_t ret = SeniorUpdateAux(seniorDoMsg);
   if (ret.IsError())
   {
      LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject::SeniorUpdate():  SeniorUpdateAux() failed! [%s]\n", ret());
      return ConstMessageRef();
   }

   // Cleanup stuff here

   return seniorDoMsg;  // todo:  return generated JuniorMsg instead!
}

status_t MessageTreeDatabaseObject :: SeniorUpdateAux(const ConstMessageRef & msg)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   switch(msg()->what)
   {
      case MTDO_SENIOR_COMMAND_BATCH:
      {
         status_t ret;
         MessageRef subMsg;
         for (int32 i=0; msg()->FindMessage(MTDO_NAME_SUBMESSAGE, i, subMsg).IsOK(); i++) 
         {
            ret = SeniorUpdateAux(subMsg);
            if (ret.IsError()) return ret;
         }
         return B_NO_ERROR;
      }
      break;

      case MTDO_SENIOR_COMMAND_UPLOADNODEVALUE:
      {
         const String * pPath     = msg()->GetStringPointer(MTDO_NAME_PATH);
         MessageRef optPayload    = msg()->GetMessage(MTDO_NAME_PAYLOAD);
         TreeGatewayFlags flags   = msg()->GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
         const String * optBefore = msg()->GetStringPointer(MTDO_NAME_BEFORE);

         return zsh->SetDataNode(pPath?*pPath:GetEmptyString(), optPayload, true, true, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY), flags.IsBitSet(TREE_GATEWAY_FLAG_INDEXED), optBefore);
      }
      break;

      case MTDO_SENIOR_COMMAND_REQUESTDELETENODES:
      {
         MessageRef qfMsg;
         TreeGatewayFlags flags    = msg()->GetFlat<TreeGatewayFlags>(MTDO_NAME_FLAGS);
         const String * pPath      = msg()->GetStringPointer(MTDO_NAME_PATH);
         const String & path       = pPath ? *pPath : GetEmptyString();
         ConstQueryFilterRef qfRef = (msg()->FindMessage(MTDO_NAME_FILTER, qfMsg).IsOK()) ? GetGlobalQueryFilterFactory()()->CreateQueryFilter(*qfMsg()) : QueryFilterRef();

         return zsh->RemoveDataNodes(path, qfRef, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY));
      }
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "MessageTreeDatabaseObject::SeniorUpdateAux():  Unknown Message code " UINT32_FORMAT_SPEC "\n", msg()->what);
         return B_UNIMPLEMENTED;
   }
}

status_t MessageTreeDatabaseObject :: JuniorUpdate(const ConstMessageRef & juniorDoMsg)
{
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   // TODO IMPLEMENT THIS

   return B_NO_ERROR;
}

String MessageTreeDatabaseObject :: ToString() const
{
   const MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return "<no database peer session!>";

   String ret;
   const DataNode * rootNode = zsh->GetDataNode(_rootNodePath);
   if (rootNode) DumpDescriptionToString(*rootNode, ret, 0);
   return ret;
}

void MessageTreeDatabaseObject :: DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const
{
   // TODO IMPLEMENT THIS
}

status_t MessageTreeDatabaseObject :: UploadNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
   MessageRef cmdMsg = GetMessageFromPool(MTDO_SENIOR_COMMAND_UPLOADNODEVALUE);
   if (cmdMsg() == NULL) RETURN_OUT_OF_MEMORY;

   const status_t ret = cmdMsg()->CAddString( MTDO_NAME_PATH,    path)
                      | cmdMsg()->CAddMessage(MTDO_NAME_PAYLOAD, optPayload)
                      | cmdMsg()->AddFlat(    MTDO_NAME_FLAGS,   flags)
                      | cmdMsg()->CAddString( MTDO_NAME_BEFORE,  optBefore);

   return ret.IsOK() ? RequestUpdateDatabaseState(cmdMsg) : ret;
}

status_t MessageTreeDatabaseObject :: RequestDeleteNodes(const String & path, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags)
{
   MessageRef cmdMsg = GetMessageFromPool(MTDO_SENIOR_COMMAND_REQUESTDELETENODES);
   if (cmdMsg() == NULL) RETURN_OUT_OF_MEMORY;

   status_t ret;
   if ((optFilter())&&(cmdMsg()->AddArchiveMessage(MTDO_NAME_FILTER, *optFilter()).IsError(ret))) return ret;

   ret = cmdMsg()->CAddString(MTDO_NAME_PATH,   path)
       | cmdMsg()->AddFlat(   MTDO_NAME_FLAGS,  flags);

   return ret.IsOK() ? RequestUpdateDatabaseState(cmdMsg) : ret;
}


}; // end namespace zg

#include "zg/messagetree/server/MessageTreeDatabasePeerSession.h"
#include "zg/messagetree/server/MessageTreeDatabaseObject.h"

namespace zg 
{

MessageTreeDatabaseObject :: MessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, const String & rootNodePath) 
   : IDatabaseObject(session)
   , _rootNodePath(rootNodePath)
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
   MessageTreeDatabasePeerSession * zsh = GetMessageTreeDatabasePeerSession();
   if (zsh == NULL) return ConstMessageRef();

   // TODO IMPLEMENT THIS

   return seniorDoMsg;
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

}; // end namespace zg

#include "zg/ZGMessageTreeDatabaseObject.h"
#include "zg/ZGDatabasePeerSession.h"

namespace zg 
{

void ZGMessageTreeDatabaseObject :: SetToDefaultState()
{
   ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh) (void) zsh->RemoveDataNodes(_rootNodePath);
}

status_t ZGMessageTreeDatabaseObject :: SetFromArchive(const ConstMessageRef & archive)
{
   ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   return zsh->RestoreNodeTreeFromMessage(*archive(), _rootNodePath, true, true);
}

status_t ZGMessageTreeDatabaseObject :: SaveToArchive(const MessageRef & archive) const
{
   const ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   const DataNode * rootNode = zsh->GetDataNode(_rootNodePath);
   return rootNode ? zsh->SaveNodeTreeToMessage(*archive(), rootNode, GetEmptyString(), true) : B_NO_ERROR;
}

uint32 ZGMessageTreeDatabaseObject :: CalculateChecksum() const
{
   const ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh == NULL) return 0;

   const DataNode * rootNode = zsh->GetDataNode(_rootNodePath);
   return rootNode ? rootNode->CalculateChecksum() : 0;
}

ConstMessageRef ZGMessageTreeDatabaseObject :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh == NULL) return ConstMessageRef();

   // TODO IMPLEMENT THIS

   return seniorDoMsg;
}

status_t ZGMessageTreeDatabaseObject :: JuniorUpdate(const ConstMessageRef & juniorDoMsg)
{
   ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh == NULL) return B_BAD_OBJECT;

   // TODO IMPLEMENT THIS

   return B_NO_ERROR;
}

String ZGMessageTreeDatabaseObject :: ToString() const
{
   const ZGDatabasePeerSession * zsh = GetDatabasePeerSession();
   if (zsh == NULL) return "<no database peer session!>";

   String ret;
   const DataNode * rootNode = zsh->GetDataNode(_rootNodePath);
   if (rootNode) DumpDescriptionToString(*rootNode, ret, 0);
   return ret;
}

void ZGMessageTreeDatabaseObject :: DumpDescriptionToString(const DataNode & node, String & s, uint32 indentLevel) const
{
   // TODO IMPLEMENT THIS
}

}; // end namespace zg

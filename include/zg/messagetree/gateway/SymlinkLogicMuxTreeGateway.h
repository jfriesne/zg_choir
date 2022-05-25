#ifndef SymlinkLogicMuxTreeGateway_h
#define SymlinkLogicMuxTreeGateway_h

#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "util/Hashtable.h"
#include "util/NestCount.h"

namespace zg {

#define SYMLINK_FIELD_NAME "__symlink"  ///< This gateway will snoop on incoming Messages and when it sees this string field, treat the node as a symlink to the path specified here

class SymlinkResolver;

/** This class contains logic to implement "symlinked nodes"
  */
class SymlinkLogicMuxTreeGateway : public MuxTreeGateway
{
public:
   /** Constructor
     * @param optUpstreamGateway if non-NULL, this is a pointer to the "upstream" gateway that we will pass our subscribers' request on to, and receive replies back from
     */
   SymlinkLogicMuxTreeGateway(ITreeGateway * optUpstreamGateway);

   /** Destructor */
   virtual ~SymlinkLogicMuxTreeGateway();

protected:
   //  ITreeGateway function-call API
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual void ReceivedPathDropped(ITreeGatewaySubscriber * /*calledBy*/, const String & receivedPath);

   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg, const String & optOpTag);

private:
   friend class SymlinkResolver;

   void FlushDroppedSymlinkPaths();

   SymlinkResolver * _symlinkResolver;
   Hashtable<String, Void> _droppedSymlinkPaths;
   NestCount _inRemoveSubscription;
   NestCount _inFlushSymlinkPaths;
};

};  // end namespace zg

#endif

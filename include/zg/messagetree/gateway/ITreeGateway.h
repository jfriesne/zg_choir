#ifndef ITreeGateway_h
#define ITreeGateway_h

#include "zg/gateway/IGateway.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "message/Message.h"
#include "regex/QueryFilter.h"

namespace zg {

class ITreeGatewaySubscriber;

/** Abstract base class for objects that want to implement the ITreeGateway API */
class ITreeGateway : public IGateway<ITreeGatewaySubscriber, ITreeGateway>
{
public:
   /** Constructor */
   ITreeGateway() {/* empty */}

   /** Destructor */
   virtual ~ITreeGateway() {/* empty */}

protected:
   // ITreeGateway function-call API -- called by the corresponding methods in the ITreeGatewaySubscriber function-call API.  See ITreeGatewaySubscriber.h for details
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags) = 0;
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore) = 0;
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const String * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags) = 0;
   virtual status_t TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB) = 0;
   virtual status_t TreeGateway_EndUndoSequence(  ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB) = 0;
   virtual status_t TreeGateway_RequestUndo(ITreeGatewaySubscriber * calledBy, uint32 whichDB) = 0;
   virtual status_t TreeGateway_RequestRedo(ITreeGatewaySubscriber * calledBy, uint32 whichDB) = 0;
   virtual bool TreeGateway_IsGatewayConnected() const = 0;

   // Pass-throughs to private methods on our IGatewaySubscriber class (used to control access to these methods, so that only their gateway can call from them)
   bool CallBeginCallbackBatch(ITreeGatewaySubscriber * s) {return s->BeginCallbackBatch();}
   bool CallEndCallbackBatch(  ITreeGatewaySubscriber * s) {return s->EndCallbackBatch();}

private:
   friend class ITreeGatewaySubscriber;
};

};  // end namespace zg

#endif

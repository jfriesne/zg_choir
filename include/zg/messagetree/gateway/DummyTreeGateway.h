#ifndef DummyTreeGateway_h
#define DummyTreeGateway_h

#include "zg/messagetree/gateway/ITreeGateway.h"

namespace zg {

/** Returns a pointer to a singleton DummyTreeGateway object, which is used to cleanly implement no-ops and return errors on all methods */
ITreeGateway * GetDummyTreeGateway();

/** Dummy subclass of ITreeGateway; all methods are implemented as no-ops. */
class DummyTreeGateway : public ITreeGateway
{
public:
   /** Constructor
     * @param returnValue the value all of our status_t-returning methods should return.  Defaults to B_UNIMPLEMENTED.
     */
   DummyTreeGateway(status_t returnValue = B_UNIMPLEMENTED) : _returnValue(returnValue) {/* empty */}

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber *, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber *, const Queue<String> &, const Queue<ConstQueryFilterRef> &, const String &, uint32, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber *, const String &, const MessageRef &, TreeGatewayFlags, const String *) {return _returnValue;}
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber *, const String &, const MessageRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber *, const String &, const String *, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_PingLocalPeer(ITreeGatewaySubscriber *, const String &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber *, const String &, uint32, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber *, const MessageRef &, uint32, const String &) {return _returnValue;}
   virtual status_t TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber *, const String &, const MessageRef &, const String &) {return _returnValue;}
   virtual status_t TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber *, const String &, uint32) {return _returnValue;}
   virtual status_t TreeGateway_EndUndoSequence(  ITreeGatewaySubscriber *, const String &, uint32) {return _returnValue;}
   virtual status_t TreeGateway_RequestUndo(ITreeGatewaySubscriber *, uint32) {return _returnValue;}
   virtual status_t TreeGateway_RequestRedo(ITreeGatewaySubscriber *, uint32) {return _returnValue;}
   virtual bool TreeGateway_IsGatewayConnected() const {return false;}

private:
   const status_t _returnValue;
};

};  // end namespace zg

#endif

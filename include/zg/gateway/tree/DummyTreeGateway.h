#ifndef DummyTreeGateway_h
#define DummyTreeGateway_h

#include "zg/gateway/tree/ITreeGateway.h"

namespace zg {

/** Returns a pointer to a singleton DummyTreeGateway object, which is used to cleanly implement no-ops and return errors on all methods */
ITreeGateway * GetDummyTreeGateway();

/** Dummy subclass of ITreeGateway; everything is a on-op */
class DummyTreeGateway : public ITreeGateway
{
public:
   DummyTreeGateway(status_t returnValue = B_UNIMPLEMENTED) : _returnValue(returnValue) {/* empty */}

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber *) {return _returnValue;}
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber *, const Queue<String> &, const Queue<ConstQueryFilterRef> &, const String &, uint32, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber *, const String &, const MessageRef &, TreeGatewayFlags, const char *) {return _returnValue;}
   virtual status_t TreeGateway_UploadNodeValues(ITreeGatewaySubscriber *, const String &, const MessageRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber *, const String &, const ConstQueryFilterRef &, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber *, const String &, const char *, TreeGatewayFlags) {return _returnValue;}
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber *, const String &, TreeGatewayFlags) {return _returnValue;}
   virtual bool TreeGateway_IsGatewayConnected() const {return false;}

private:
   const status_t _returnValue;
};

};  // end namespace zg

#endif

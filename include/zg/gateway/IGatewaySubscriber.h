#ifndef IGatewaySubscriber_h
#define IGatewaySubscriber_h

#include "zg/ZGNameSpace.h"
#include "support/NotCopyable.h"

namespace zg {

/** Abstract base class for objects that want to interact with a gateway's public API. */
template<class GatewayType> class IGatewaySubscriber : public NotCopyable
{
public:
   IGatewaySubscriber(GatewayType * gateway) : _gateway(NULL) {SetGateway(gateway);}
   virtual ~IGatewaySubscriber() {SetGateway(NULL);}

   void SetGateway(GatewayType * optGateway);
   GatewayType * GetGateway() const {return _gateway;}

   virtual void BeginCommandBatch();
   virtual void EndCommandBatch();

   // Called before zero or more calls to our callback-methods
   virtual void CallbackBatchBegins() {/* empty */}

   // Called after zero or more calls to our callback-methods
   virtual void CallbackBatchEnds() {/* empty */}

private:
   GatewayType * _gateway;
};

/** RIAA stack-guard object to begin and end an IGateway's Command Batch at the appropriate times */
template<class SubscriberType> class GatewaySubscriberCommandBatchGuard : public NotCopyable
{
public:
   GatewaySubscriberCommandBatchGuard(SubscriberType * sub) : _sub(sub) {_sub->BeginCommandBatch();}
   ~GatewaySubscriberCommandBatchGuard() {_sub->EndCommandBatch();}

private:
   SubscriberType * _sub;
};

};  // end namespace zg

#endif

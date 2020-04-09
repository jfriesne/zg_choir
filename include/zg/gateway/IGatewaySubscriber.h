#ifndef IGatewaySubscriber_h
#define IGatewaySubscriber_h

#include "zg/gateway/IGateway.h"

namespace zg {

/** Abstract base class for objects that want to interact with a gateway's public API. */
template<class GatewayType, class SubscriberType> class IGatewaySubscriber : public NotCopyable
{
public:
   IGatewaySubscriber(GatewayType * gateway) : _gateway(NULL) {SetGateway(gateway);}
   virtual ~IGatewaySubscriber() {SetGateway(NULL);}

   void SetGateway(GatewayType * optGateway);

   GatewayType * GetGateway() const {return _gateway;}

   virtual void BeginCommandBatch() {if (_gateway) _gateway->BeginCommandBatch(static_cast<SubscriberType *>(this));}
   virtual void EndCommandBatch()   {if (_gateway) _gateway->EndCommandBatch(static_cast<SubscriberType *>(this));}

private:
   GatewayType * _gateway;
};

// Implemented here to resolve cyclic dependency
template<class GatewaySubscriberBaseType, class GatewaySubscriberType> void IGateway<GatewaySubscriberBaseType, GatewaySubscriberType>::ShutdownGateway()
{
   while(_registeredSubscribers.HasItems()) _registeredSubscribers.GetFirstKeyWithDefault()->SetGateway(NULL);
}

/** RIAA stack-guard object to begin and end an IGateway's Command Batch at the appropriate times */
template<class SubscriberType> class SubscriberCommandBatchGuard : public NotCopyable
{
public:
   SubscriberCommandBatchGuard(SubscriberType * sub) : _sub(sub) {_sub->BeginCommandBatch();}
   ~SubscriberCommandBatchGuard() {_sub->EndCommandBatch();}

private:
   SubscriberType * _sub;
};

template<class GatewayType, class SubscriberType> void IGatewaySubscriber<GatewayType, SubscriberType> :: SetGateway(GatewayType * optGateway)
{
   if (optGateway != _gateway)
   {
      if (_gateway) _gateway->UnregisterSubscriber(static_cast<SubscriberType *>(this));
      _gateway = optGateway;
      if (_gateway) _gateway->RegisterSubscriber(static_cast<SubscriberType *>(this));
   }
}

};  // end namespace zg

#endif

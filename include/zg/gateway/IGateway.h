#ifndef IGateway_h
#define IGateway_h

#include "zg/ZGNameSpace.h"
#include "zg/gateway/IGatewaySubscriber.h"
#include "support/NotCopyable.h"
#include "util/Hashtable.h"
#include "util/NestCount.h"

namespace zg {

class ITreeGateway;

/** Abstract base class for objects that want to server as a gateway to a number of IGatewaySubscriber objects 
 *  The GatewaySubscriberType template-argument should be either IGatewaySubscriber or a subclass thereof.
 *  The GatewaySubclass template-argument should be type of the gateway subclass itself (just so we can declare IGatewaySubscriber<GatewaySubclass> as a friend, argh)
 */
template<class GatewaySubscriberType, class GatewaySubclass> class IGateway : public NotCopyable
{
public:
   IGateway() {/* empty */}
   virtual ~IGateway() {MASSERT(_registeredSubscribers.IsEmpty(), "IGateway was destroyed without calling ShutdownGateway() on it first!");}

   virtual bool BeginCommandBatch() {const bool ret = _commandBatchCounter.Increment();   if (ret) CommandBatchBegins();    return ret;}
   virtual bool   EndCommandBatch() {const bool ret = _commandBatchCounter.IsOutermost(); if (ret) CommandBatchEnds();      _commandBatchCounter.Decrement();  return ret;}

   virtual bool BeginCallbackBatch() {const bool ret = _callbackBatchCounter.Increment();   if (ret) CallbackBatchBegins(); return ret;}
   virtual bool   EndCallbackBatch() {const bool ret = _callbackBatchCounter.IsOutermost(); if (ret) CallbackBatchEnds();   _callbackBatchCounter.Decrement(); return ret;}

   bool  IsInCommandBatch() const {return _commandBatchCounter.IsInBatch();}
   bool IsInCallbackBatch() const {return _callbackBatchCounter.IsInBatch();}

   virtual void ShutdownGateway() {while(_registeredSubscribers.HasItems()) _registeredSubscribers.GetFirstKeyWithDefault()->SetGateway(NULL);}

protected:
   /** Called when our command-batch counter has just gone from 0 to 1. */
   virtual void CommandBatchBegins() {/* empty */}

   /** Called when our command-batch counter is just about to go from 1 to 0. */
   virtual void CommandBatchEnds() {/* empty */}

   /** Called when our callback-batch counter has just gone from 0 to 1. */
   virtual void CallbackBatchBegins() {/* empty */}

   /** Called when our callback-batch counter is just about to go from 1 to 0. */
   virtual void CallbackBatchEnds() {/* empty */}

   /** Returns a read-only reference to our table of currently-registered IGatewaySubscriber objects */
   const Hashtable<GatewaySubscriberType *, Void> & GetRegisteredSubscribers() const {return _registeredSubscribers;}

protected:
   virtual void   RegisterSubscriber(void * s) {(void) _registeredSubscribers.PutWithDefault(static_cast<GatewaySubscriberType *>(s));}
   virtual void UnregisterSubscriber(void * s) {(void) _registeredSubscribers.Remove(static_cast<GatewaySubscriberType *>(s));}
   
private:
   friend class IGatewaySubscriber<GatewaySubclass>;

   Hashtable<GatewaySubscriberType *, Void> _registeredSubscribers;
   NestCount _commandBatchCounter;
   NestCount _callbackBatchCounter;
};

// Implemented here to resolve cyclic dependency
template<class GatewayType> void IGatewaySubscriber<GatewayType>::BeginCommandBatch() {if (_gateway) _gateway->BeginCommandBatch();}
template<class GatewayType> void IGatewaySubscriber<GatewayType>::EndCommandBatch()   {if (_gateway) _gateway->EndCommandBatch();}
template<class GatewayType> void IGatewaySubscriber<GatewayType> :: SetGateway(GatewayType * optGateway)
{
   if (optGateway != _gateway)
   {
      if (_gateway) _gateway->UnregisterSubscriber(this);
      _gateway = optGateway;
      if (_gateway) _gateway->RegisterSubscriber(this);
   }
}

/** RIAA stack-guard object to begin and end an IGateway's Callback Batch at the appropriate times */
template<class GatewayType> class GatewayCallbackBatchGuard : public NotCopyable
{
public:
   GatewayCallbackBatchGuard(GatewayType * ig) : _gateway(ig) {_gateway->BeginCallbackBatch();}
   ~GatewayCallbackBatchGuard() {_gateway->EndCallbackBatch();}

private:
   GatewayType * _gateway;
};

};  // end namespace zg

#endif

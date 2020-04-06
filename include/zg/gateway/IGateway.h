#ifndef IGateway_h
#define IGateway_h

#include "zg/ZGNameSpace.h"
#include "support/NotCopyable.h"
#include "util/Hashtable.h"
#include "util/NestCount.h"

namespace zg {

class IGateway;

/** Abstract base class for objects that want to interact with a gateway's public API. */
class IGatewaySubscriber : public NotCopyable
{
protected:
   IGatewaySubscriber(IGateway * gateway) : _gateway(NULL) {SetGateway(gateway);}
   virtual ~IGatewaySubscriber() {SetGateway(NULL);}

   void SetGateway(IGateway * g);
   IGateway * GetGateway() const {return _gateway;}

private:
   friend class IGateway;

   IGateway * _gateway;
};

/** Abstract base class for objects that want to server as a gateway to a number of IGatewaySubscriber objects */
class IGateway : public NotCopyable
{
public:
   IGateway() {/* empty */}
   virtual ~IGateway() {MASSERT(_registeredSubscribers.IsEmpty(), "IGateway was destroyed without calling ShutdownGateway() on it first!");}

   virtual bool BeginCommandBatch() {const bool ret = _commandBatchCounter.Increment();   if (ret) CommandBatchBegins(); return ret;}
   virtual bool   EndCommandBatch() {const bool ret = _commandBatchCounter.IsOutermost(); if (ret) CommandBatchEnds();   _commandBatchCounter.Decrement(); return ret;}

   virtual bool BeginCallbackBatch() {const bool ret = _callbackBatchCounter.Increment();  if (ret) CallbackBatchBegins(); return ret;}
   virtual bool   EndCallbackBatch() {const bool ret = _callbackBatchCounter.IsOutermost(); if (ret) CallbackBatchEnds(); _callbackBatchCounter.Decrement(); return ret;}

   bool  IsInCommandBatch() const {return _commandBatchCounter.IsInBatch();}
   bool IsInCallbackBatch() const {return _callbackBatchCounter.IsInBatch();}

   virtual void ShutdownGateway()
   {
      while(_registeredSubscribers.HasItems()) _registeredSubscribers.GetFirstKeyWithDefault()->SetGateway(NULL);
   }

protected:
   /** Called when our command-batch counter has just gone from 0 to 1. */
   virtual void CommandBatchBegins() {/* empty */}

   /** Called when our command-batch counter is just about to go from 1 to 0. */
   virtual void CommandBatchEnds() {/* empty */}

   /** Called when our callback-batch counter has just gone from 0 to 1. */
   virtual void CallbackBatchBegins() {/* empty */}

   /** Called when our callback-batch counter is just about to go from 1 to 0. */
   virtual void CallbackBatchEnds() {/* empty */}

   /** called when a new IGatewaySubscriber wants to begin an association with us. */
   virtual void   RegisterSubscriber(IGatewaySubscriber * s) {(void) _registeredSubscribers.PutWithDefault(s);}

   /** called when an existingIGatewaySubscriber wants to end its association with us. */
   virtual void UnregisterSubscriber(IGatewaySubscriber * s) {(void) _registeredSubscribers.Remove(s);}
   
   /** Returns a read-only reference to our table of currently-registered IGatewaySubscriber objects */
   const Hashtable<IGatewaySubscriber *, Void> & GetRegisteredSubscribers() const {return _registeredSubscribers;}

private:
   friend class IGatewaySubscriber;

   Hashtable<IGatewaySubscriber *, Void> _registeredSubscribers;
   NestCount _commandBatchCounter;
   NestCount _callbackBatchCounter;
};
DECLARE_REFTYPES(IGateway);

/** RIAA stack-guard object to begin and end an IGateway's Command Batch at the appropriate times */
class GatewayCommandBatchGuard : public NotCopyable
{
public:
   GatewayCommandBatchGuard(IGateway * ig) : _gateway(ig) {_gateway->BeginCommandBatch();}
   ~GatewayCommandBatchGuard() {_gateway->EndCommandBatch();}

private:
   IGateway * _gateway;
};

/** RIAA stack-guard object to begin and end an IGateway's Callback Batch at the appropriate times */
class GatewayCallbackBatchGuard : public NotCopyable
{
public:
   GatewayCallbackBatchGuard(IGateway * ig) : _gateway(ig) {_gateway->BeginCallbackBatch();}
   ~GatewayCallbackBatchGuard() {_gateway->EndCallbackBatch();}

private:
   IGateway * _gateway;
};

};  // end namespace zg

#endif

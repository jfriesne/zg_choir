#ifndef IGateway_h
#define IGateway_h

#include "zg/ZGNameSpace.h"
#include "zg/gateway/IGatewaySubscriber.h"
#include "support/NotCopyable.h"
#include "util/Hashtable.h"
#include "util/NestCount.h"

namespace zg {

class ITreeGateway;

extern ITreeGateway * GetDummyTreeGateway();

/** Abstract base class for objects that want to server as a gateway to a number of IGatewaySubscriber objects 
 *  The GatewaySubscriberType template-argument should be either IGatewaySubscriber or a subclass thereof.
 *  The GatewaySubclass template-argument should be type of the gateway subclass itself (just so we can declare IGatewaySubscriber<GatewaySubclass> as a friend, argh)
 */
template<class GatewaySubscriberType, class GatewaySubclass> class IGateway : public NotCopyable
{
public:
   IGateway() : _registrationIDCounter(0) {/* empty */}
   virtual ~IGateway() {MASSERT(_registeredSubscribers.IsEmpty(), "IGateway was destroyed without calling ShutdownGateway() on it first!");}

   bool BeginCommandBatch()  {const bool ret = _commandBatchCounter.Increment();    if (ret) CommandBatchBegins();  return ret;}
   bool   EndCommandBatch()  {const bool ret = _commandBatchCounter.IsOutermost();  if (ret) CommandBatchEnds();    _commandBatchCounter.Decrement();  return ret;}

   bool BeginCallbackBatch() {const bool ret = _callbackBatchCounter.Increment();   if (ret) CallbackBatchBegins(); return ret;}
   bool   EndCallbackBatch() {const bool ret = _callbackBatchCounter.IsOutermost(); if (ret) CallbackBatchEnds();   _callbackBatchCounter.Decrement(); return ret;}

   bool  IsInCommandBatch() const {return _commandBatchCounter.IsInBatch();}
   bool IsInCallbackBatch() const {return _callbackBatchCounter.IsInBatch();}

   virtual void ShutdownGateway() {while(_registeredSubscribers.HasItems()) _registeredSubscribers.GetFirstKeyWithDefault()->SetGateway(GetDummyTreeGateway());}

protected:
   /** Called when our command-batch counter has just gone from 0 to 1.
     * Default implementation is a no-op.
     */
   virtual void CommandBatchBegins() {/* empty */}

   /** Called when our command-batch counter is just about to go from 1 to 0.
     * Default implementation is a no-op.
     */
   virtual void CommandBatchEnds() {/* empty */}

   /** Called when our callback-batch counter has just gone from 0 to 1. */
   virtual void CallbackBatchBegins() {/* empty */}

   /** Called when our callback-batch counter is just about to go from 1 to 0. */
   virtual void CallbackBatchEnds() {/* empty */}

   /** Returns a read-only reference to our table of currently-registered IGatewaySubscriber objects, mapped to their registration IDs */
   const Hashtable<GatewaySubscriberType *, uint32> & GetRegisteredSubscribers() const {return _registeredSubscribers;}

   /** Returns a read-only reference to our table of current registration IDs, mapped to their subscribers */
   const Hashtable<uint32, GatewaySubscriberType *> & GetRegistrationIDs() const {return _registrationIDs;}

protected:
   virtual void RegisterSubscriber(void * s) 
   {
      const uint32 registrationID = ++_registrationIDCounter;
      GatewaySubscriberType * sub = static_cast<GatewaySubscriberType *>(s);
      if ((_registeredSubscribers.Put(sub, registrationID).IsError())||(_registrationIDs.Put(registrationID, sub).IsError())) MCRASH("IGateway::RegisterSubscriber:  Registration failed!");
   }

   virtual void UnregisterSubscriber(void * s) 
   {
      GatewaySubscriberType * sub = static_cast<GatewaySubscriberType *>(s);
      uint32 registrationID = 0;  // just to avoid a compiler warning
      if (_registeredSubscribers.Remove(sub, registrationID).IsOK()) (void) _registrationIDs.Remove(registrationID);
                                                                else MCRASH("IGateway::UnregisterSubscriber:  Unknown subscriber!");
   }
   
private:
   friend class IGatewaySubscriber<GatewaySubclass>;

   uint32 _registrationIDCounter;
   Hashtable<GatewaySubscriberType *, uint32> _registeredSubscribers;   // subscriber -> registrationID
   Hashtable<uint32, GatewaySubscriberType *> _registrationIDs;         // registrationID -> subscriber
   NestCount _commandBatchCounter;
   NestCount _callbackBatchCounter;
};

// Implemented here to resolve cyclic dependency
template<class GatewayType> bool IGatewaySubscriber<GatewayType> :: IsGatewayInCommandBatch() const {return _gateway ? _gateway->IsInCommandBatch() : false;}
template<class GatewayType> bool IGatewaySubscriber<GatewayType> :: BeginCommandBatch() {return _gateway ? _gateway->BeginCommandBatch() : false;}
template<class GatewayType> bool IGatewaySubscriber<GatewayType> :: EndCommandBatch()   {return _gateway ? _gateway->EndCommandBatch()   : false;}
template<class GatewayType> void IGatewaySubscriber<GatewayType> :: SetGateway(GatewayType * optGateway)
{
   if (optGateway != _gateway)
   {
      // If we're about to be change gateways, then we need to unwind our callback-batch-state
      // right now, since after this our gateway will no longer be around to call EndCallbackBatch() for us!
      while(_callbackBatchCounter.IsInBatch()) EndCallbackBatch();

      if (_gateway) _gateway->UnregisterSubscriber(this);
      _gateway = optGateway;
      if (_gateway) _gateway->RegisterSubscriber(this);
   }
}

template<class GatewayType> bool IGatewaySubscriber<GatewayType>::BeginCallbackBatch()
{
   const bool ret = _callbackBatchCounter.Increment();
   if (ret) 
   {
      (void) BeginCommandBatch();   // might as well auto-enter a command-batch when we start the callback batch
      CallbackBatchBegins();        // that way the user-code doesn't have to remember when to do so
   } 
   return ret;
}

template<class GatewayType> bool IGatewaySubscriber<GatewayType>::EndCallbackBatch()
{
   const bool ret = _callbackBatchCounter.IsOutermost();
   if (ret) 
   {
      CallbackBatchEnds();
      (void) EndCommandBatch();
   }
   _callbackBatchCounter.Decrement();
   return ret;
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

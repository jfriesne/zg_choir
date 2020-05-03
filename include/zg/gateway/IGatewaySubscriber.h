#ifndef IGatewaySubscriber_h
#define IGatewaySubscriber_h

#include "zg/ZGNameSpace.h"
#include "support/NotCopyable.h"
#include "util/NestCount.h"

namespace zg {

/** Abstract base class for objects that want to interact with a gateway's public API. */
template<class GatewayType> class IGatewaySubscriber : public NotCopyable
{
public:
   /** Constructor
     * @param optGateway pointer to the gateway this subscriber should register with.  May be NULL
     *                   if you don't want to register right away (in which case you'll probably want
     *                   to call SetGateway() on this object later)
     */
   IGatewaySubscriber(GatewayType * optGateway) : _gateway(NULL) {SetGateway(optGateway);}

   /** Destructor -- unregisters this subscriber from its gateway */
   virtual ~IGatewaySubscriber() {SetGateway(NULL);}

   /** Sets this subscriber to be registered with the specified gateway object
     * @param optGateway pointer to the gateway to register with, or NULL to unregister with any existing gateway only.
     * @note this method is careful to unregister with any existing gateway before registering with the new gateway.
     */
   void SetGateway(GatewayType * optGateway);

   /** Returns a pointer to the gateway this subscriber is currently registered with. */
   GatewayType * GetGateway() const {return _gateway;}

   /** User code can call this method to indicate that it is planning to make one or more command-calls on the gateway in
     * the near future.  Making command-calls within a batch allows the gateway to make some efficiency-optimizations
     * that would otherwise be difficult to make.
     * @returns true if this is the beginning of the command-batch, or, false if we were already in batch-mode (in which case
     *               this call simply increments an internal counter to note the additional level of nesting)
     * @note it's often preferable to declare a GatewaySubscriberCommandBatchGuard object on the stack, instead of calling this method directly,
     *       because that way it's impossible for you to forget to call the corresponding EndCommandBatch() when you're done calling command-methods.
     */
   bool BeginCommandBatch();

   /** User code can call this method to indicate that it has finished the sequence of command-calls on the gatewya.
     * You should call this exactly once for each time you call BeginCommandBatch().
     * @returns true if this call marks the end of the command-batch, or, false if the batch-mode nesting-counter was greater than 1
     *               (in which case this call simply decrements an internal counter to note the removal of a level of nesting)
     * @note it's often preferable to declare a GatewaySubscriberCommandBatchGuard object on the stack, so that you don't need to explicitly call this method.
     */
   bool EndCommandBatch();

protected:
   /** This callback method is called before zero or more calls to our callback-methods are made by our upstream gateway.
     * Default implementation is a no-op.
     */
   virtual void CallbackBatchBegins() {/* empty */}

   /** Called after zero or more calls to our callback-methods have been made by our upstream gateway.
     * Default implementation is a no-op.
     */
   virtual void CallbackBatchEnds() {/* empty */}

   /** Returns true iff we are currently executing inside a callback-batch context */
   bool IsInCallbackBatch() const {return _callbackBatchCounter.IsInBatch();}

   /** Returns true iff our gateway is currently executing inside a command-batch context */
   bool IsGatewayInCommandBatch() const;

private:
   friend GatewayType;

   // These methods are to be called by our gateway ONLY!  (since it is our gateway who is in charge of making callbacks on us)
   bool BeginCallbackBatch();
   bool EndCallbackBatch();

   GatewayType * _gateway;
   NestCount _callbackBatchCounter;
};

/** RIAA stack-guard object to begin and end an IGateway's Command Batch at the appropriate times */
template<class SubscriberType> class GatewaySubscriberCommandBatchGuard : public NotCopyable
{
public:
   /** Constructor
     * @param sub pointer to the subscriber object to call BeginCommandBatch() on
     */
   GatewaySubscriberCommandBatchGuard(SubscriberType * sub) : _sub(sub) {_sub->BeginCommandBatch();}

   /** Destructor
     * Calls EndCommandBatch() on the subscriber object
     */
   ~GatewaySubscriberCommandBatchGuard() {_sub->EndCommandBatch();}

private:
   SubscriberType * _sub;
};

};  // end namespace zg

#endif

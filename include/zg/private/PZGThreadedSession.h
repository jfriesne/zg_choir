#ifndef PZGThreadedSession_h
#define PZGThreadedSession_h

#include "reflector/AbstractReflectSession.h"
#include "system/Thread.h"
#include "zg/private/PZGNameSpace.h"

namespace zg_private
{

class IThreadedMaster;

enum {
   PZG_THREADED_SESSION_RECREATE_SOCKETS = 1887073384 // 'pzth' 
};

/** This semi-abstract class represents a session that is managing an internal thread.  
  * The internal thread may communicate only with this session; it is hidden from the rest of the process.
  *
  * This class is intended to be subclassed in order to implement specific functionality.
  *
  */
class PZGThreadedSession : public AbstractReflectSession, protected Thread
{
public:
   /** Default constructor */
   PZGThreadedSession();

   /** Overridden to return the owner-wakeup-socket from the Thread class, so this session will monitor that. */
   virtual ConstSocketRef CreateDefaultSocket();

   /** Overridden to create a SignalMessageIOGateway that will be used to send signals to/from the internal Thread. */
   virtual AbstractMessageIOGatewayRef CreateGateway();

   /** Overridden to start up the internal thread before attaching. */
   virtual status_t AttachedToServer();

   /** Overridden to shut down the internal thread before detaching. */
   virtual void AboutToDetachFromServer();

   /** Overridden to shut down the internal thread ASAP (just to avoid any potential race conditions). */
   virtual void EndSession();

   /** Sends a Message to our internal thread requesting it to recreate its multicast socket to reflect a change in network configuration. */
   status_t TellInternalThreadToRecreateMulticastSockets();

protected:
   /** Overridden to check our queue of Messages coming from the internal Thread (in response to a signal from the Thread) */
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

   /** Must be implemented by the subclass to handle the Message that was received from the internal thread. */
   virtual void MessageReceivedFromInternalThread(const MessageRef & msgFromInternalThread) = 0;
};

};  // end namespace zg_private

#endif

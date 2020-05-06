#ifndef ZGStdinSession_h
#define ZGStdinSession_h

#include "reflector/AbstractReflectSession.h"
#include "zg/ITextCommandReceiver.h"

namespace zg
{

/** This is a utility class that knows how to read from the process's
  * stdin and hand any received text commands over to the TextCommandReceived()
  * method of the ITextCommandReceiver that is passed to its constructor.
  * (The ITextCommandReceiver would typically be your ZGPeerSession object,
  * but it's possible to use this class with any ITextCommandReceiver-derived
  * object as its target.
  */
class ZGStdinSession : public AbstractReflectSession
{
public:
   /** Constructor
     * @param target The object to call TextCommandReceived() on when a line of text is received from stdin
     * @param endServerOnClose if true, we'll call EndServer() when stdin is closed, to cause this process to exit.
     */
   ZGStdinSession(ITextCommandReceiver & target, bool endServerOnClose);

   /** Overridden to return an invalid socket.  We'll handle the socket-creation ourself, inside CreateDataIO() */
   virtual ConstSocketRef CreateDefaultSocket() {return GetInvalidSocket();}

   /** Overridden to create and return a StdinDataIO object */
   virtual DataIORef CreateDataIO(const ConstSocketRef &);

   /** Overridden to quit the ReflectServer event-loop when stdin is closed (if that behavior was specified in our constructor) */
   virtual bool ClientConnectionClosed();

   /** Returns true iff our (target)'s IsReadyForInput() method returns true. */
   virtual bool IsReadyForInput() const;

   /** Overridden to create and return a PlainTextMessageIOGateway, since stdin input will be in the form of human-readable ASCII text */
   virtual AbstractMessageIOGatewayRef CreateGateway();

   /** Overridden to handle incoming text from the PlainTextMessageIOGateway
     * @param msg The incoming MessageRef containing text strings to process
     * @param ptr Unused for now
     */
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * ptr);

   /** Implemented to return "ZGStdin" */
   virtual const char * GetTypeName() const {return "ZGStdin";}

   /** Returns true iff we will call EndServer() if stdin is closed */
   bool IsEndServerOnClose() const {return _endServerOnClose;}

   /** Sets whether or not we should call EndServer when stdin is closed. */
   void SetEndServerOnClose(bool esoc) {_endServerOnClose = esoc;}

   /** Returns the ITextCommandReceiver object that was passed into our ctor. */
   const ITextCommandReceiver & GetTextCommandReceiver() const {return _target;}

   /** Returns the ITextCommandReceiver object that was passed into our ctor. */
   ITextCommandReceiver & GetTextCommandReceiver() {return _target;}

   /** Returns true iff this object called EndServer() already */
   bool EndServerRequested() const {return _calledEndServer;}

private:
   bool IsReallyStdin() const;

   ITextCommandReceiver & _target;
   bool _endServerOnClose;
   bool _calledEndServer;
};
DECLARE_REFTYPES(ZGStdinSession);

};  // end namespace zg

#endif

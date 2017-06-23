#ifndef ZGStdinSession_h
#define ZGStdinSession_h

#include "reflector/AbstractReflectSession.h"

#include "zg/ZGNameSpace.h"

namespace zg
{

/** Interface for an object that can receive text commands from the user.
  * This interface is typically passed to a ZGStdinSession object which will
  * call its methods when appropriate.
  */
class ITextCommandReceiver
{
public:
   ITextCommandReceiver() {/* empty */}
   virtual ~ITextCommandReceiver() {/* empty */}

   /** Should be implemented to true iff this object is ready to have its
     * TextCommandReceived() method called at this time; or false if
     * the caller should wait until later.
     */
   virtual bool IsReadyForTextCommands() const = 0;

   /** Called whenever the user types in a text command.
     * @param text The text the user typed in to stdin.
     * @return true if the command was recognized and handled, or false if the command wasn't recognized.
     */
   virtual bool TextCommandReceived(const String & text) = 0;

protected:
   /** Tries to handle any text commands that can be handled generically;
     * that is, any commands that can be handled without knowing anything
     * about this particular application.  This logic is broken out into
     * a separate function so that it can be called by the stdin-text handlers
     * of various daemons.
     * @param cmd the command text to parse
     * @returns true if the command was handled, false if it wasn't.
     */
   bool ParseGenericTextCommand(const String & cmd);
};

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

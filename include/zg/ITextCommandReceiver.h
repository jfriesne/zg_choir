#ifndef ITextCommandReceiver_h
#define ITextCommandReceiver_h

#include "zg/ZGNameSpace.h"
#include "util/String.h"

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
   MUSCLE_NODISCARD virtual bool IsReadyForTextCommands() const = 0;

   /** Called whenever the user types in a text command.
     * @param text The text the user typed in to stdin.
     * @return true if the command was recognized and handled, or false if the command wasn't recognized.
     */
   MUSCLE_NODISCARD virtual bool TextCommandReceived(const String & text) = 0;

protected:
   /** Tries to handle any text commands that can be handled generically;
     * that is, any commands that can be handled without knowing anything
     * about this particular application.  This logic is broken out into
     * a separate function so that it can be called by the stdin-text handlers
     * of various daemons.
     * @param cmd the command text to parse
     * @returns true if the command was handled, false if it wasn't.
     */
   MUSCLE_NODISCARD bool ParseGenericTextCommand(const String & cmd);
};

};  // end namespace zg

#endif

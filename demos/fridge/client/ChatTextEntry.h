#ifndef ChatTextEntry_h
#define ChatTextEntry_h

#include "common/FridgeNameSpace.h"
#include "message/Message.h"

namespace fridge {

enum {
   CHATTEXTENTRY_TYPE_CODE = 1667785076 // 'chat' 
};

/** An object of this class holds the state of one line of chat text */
class ChatTextEntry
{
public:
   /** Default Constructor */
   ChatTextEntry() : _timeStamp(0) {/* empty */}

   /** Constructor
     * @param chatText what the user typed
     * @param userName the user's name, for attribution
     * @param timeStamp the time at which the chat text was generated (microseconds since 1970, local time)
     */
   ChatTextEntry(const String & chatText, const String & userName, uint64 timeStamp) : _chatText(chatText), _userName(userName), _timeStamp(timeStamp) {/* empty */}

   /** Saves the state of this object into a Message
     * @param archive the Message to save the state into
     * @returns B_NO_ERROR on success, or some other error-code on failure.
     */
   status_t SaveToArchive(Message & archive) const
   {
      archive.what = CHATTEXTENTRY_TYPE_CODE;
      return archive.CAddString("text", _chatText) 
           | archive.CAddString("user", _userName)
           | archive.CAddInt64( "time", _timeStamp);
   }

   /** Sets the state of this object from a Message
     * @param archive the Message to restore the state from
     * @returns B_NO_ERROR on success, or some other error-code on failure.
     */
   status_t SetFromArchive(const Message & archive)
   {
      if (archive.what != CHATTEXTENTRY_TYPE_CODE) return B_BAD_ARGUMENT;
      _chatText  = archive.GetString("text");
      _userName  = archive.GetString("user");
      _timeStamp = archive.GetInt64( "time");
      return B_NO_ERROR;
   }

   /** Returns -1, 0, or 1, depending on how this ChatTextEntry should be 
     * ordered with respect to (rhs).
     * @param rhs the other ChatTextEntry to compare to
     */
   int CompareTo(const ChatTextEntry & rhs) const
   {
      int ret = muscleCompare(_timeStamp, rhs._timeStamp);
      if (ret) return ret;

      ret = muscleCompare(_userName, rhs._userName);
      if (ret) return ret;

      return muscleCompare(_chatText, rhs._chatText);
   }

   /** Returns this line of text as it should appear in the chat window */
   String ToString() const
   {
      return String("%1 %2: %3").Arg(GetHumanReadableTimeString(_timeStamp, MUSCLE_TIMEZONE_LOCAL)).Arg(_userName).Arg(_chatText);
   }

private:
   String _chatText;
   String _userName; 
   uint64 _timeStamp;
};

}; // end namespace fridge

#endif

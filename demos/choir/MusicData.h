#ifndef MusicData_h
#define MusicData_h

#include "message/Message.h"
#include "util/Hashtable.h"
#include "util/String.h"

#include "zg/ZGPeerID.h"
#include "zg/IDatabaseObject.h"
#include "ChoirNameSpace.h"

namespace choir {

class ChoirSession;

/** Our enumeration of supported notes, and their numeric indices.
  * These can be used to compute bit-positions within our 64-bit chord values.
  * Note that we only define 21 notes for now; the rest of the 64 bits
  * in each chord are reserved for future expansion.
  */
enum 
{
   CHOIR_NOTE_E6 = 0, // 1318.51 (third implied-line above the top-most line of the treble staff)
   CHOIR_NOTE_D6,  // 1174.659
   CHOIR_NOTE_C6,  // 1046.502
   CHOIR_NOTE_B5,  // 987.7666
   CHOIR_NOTE_A5,  // 880
   CHOIR_NOTE_G5,  // 783.9909
   CHOIR_NOTE_F5,  // 698.4565 (top-most explicit-line of the treble staff)
   CHOIR_NOTE_E5,  // 659.2551
   CHOIR_NOTE_D5,  // 587.3295
   CHOIR_NOTE_C5,  // 523.2511
   CHOIR_NOTE_B4,  // 493.8833
   CHOIR_NOTE_A4,  // 440
   CHOIR_NOTE_G4,  // 391.9954
   CHOIR_NOTE_F4,  // 349.2282
   CHOIR_NOTE_E4,  // 329.6276 (bottom-most explicit-line of the treble staff)
   CHOIR_NOTE_D4,  // 293.6648
   CHOIR_NOTE_C4,  // 261.6256 (middle C)
   CHOIR_NOTE_B3,  // 246.9417
   CHOIR_NOTE_A3,  // 220
   CHOIR_NOTE_G3,  // 195.9977
   CHOIR_NOTE_F3,  // 174.6141 (third implied-line below the bottom-most line of the treble staff)
   NUM_CHOIR_NOTES
};

/** Given a CHOIR_NOTE_* value, returns a human-readable description of the note (e.g. "E3") */
const char * GetNoteName(uint32 noteIdx);

/** Given a peer's ID and his optional info, returns a human-readable ID string for him */
String GetPeerNickname(const ZGPeerID & pid, const ConstMessageRef & optPeerInfo);

enum {
   MUSIC_TYPE_MUSIC_SHEET = 1836413795, // 'musc' 
   MUSIC_TYPE_PLAYBACK_STATE,
   MUSIC_TYPE_ASSIGNMENTS_MAP
};

enum {
   DEFAULT_MICROSECONDS_PER_CHORD = 250000  // 4 chords/second is a reasonable default, I suppose?
};

/** Our various strategies for keeping notes assigned to bells, as peers come online and go offline */
enum {
   ASSIGNMENT_STRATEGY_AUTOMATIC = 0,  // ChoirSession will try to keep bells load-balanced at all times
   ASSIGNMENT_STRATEGY_ASSISTED,       // ChoirSession will assign unassigned bells, but otherwise leave things as they are
   ASSIGNMENT_STRATEGY_MANUAL,         // ChoirSession won't auto-assign anything; it's up the user to do it
   NUM_ASSIGNMENT_STRATEGIES
};

/** A slight specialization of the IDatabaseObject class, just so I can add some 
  * application-specific helper methods for my various subclasses to all use.  
  */
class MusicDatabaseObject : public IDatabaseObject
{
public:
   /** Default constructor */
   MusicDatabaseObject() {/* empty */}

protected:
   /** Returns a pointer to the ChoirSession object that created this object. */
   ChoirSession * GetChoirSession();

   /** Returns a read-only pointer to the ChoirSession object that created this object. */
   const ChoirSession * GetChoirSession() const;

   /** Saves the state of this object into a Message and sends that Message to the GUI thread.
     * @param allowReviewTrigger if true, this method may also trigger a review of the current bell-assignments to see if they need updating.
     * @returns a read-only reference to the Message we sent, on success, or a NULL reference on failure.
     */
   ConstMessageRef SendFullStateToGUI(bool allowReviewTrigger);

   /** Sends the specified Message to the GUI thread.
     * @param msg The Message to send to the GUI thread.
     * @param allowReviewTrigger if true, this method may also trigger a review of the current bell-assignments to see if they need updating.
     */
   void SendMessageToGUI(const ConstMessageRef & msg, bool allowReviewTrigger);

   /** Convenience method; calls SetReviewResults(allNotesUsedChord) on our ChoirSession object.
     * @param allNotesUsedChord a bit-chord of CHOIR_NOTE_* values indicating which notes are present in our current song.
     */
   void SetReviewResults(uint64 allNotesUsedChord);
};
DECLARE_REFTYPES(MusicDatabaseObject);

// Utility function to upgrade our histogram and notes-bit-chord in response to the addition or removal of a notes-chord
void UpdateNoteHistogram(uint64 chordVal, bool isAdd, Hashtable<uint8, uint32> & histogram, uint64 & bitchord);

}; // end namespace choir

#endif

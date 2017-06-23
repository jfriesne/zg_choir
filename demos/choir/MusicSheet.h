#ifndef MusicSheet_h
#define MusicSheet_h

#include "MusicData.h"

namespace choir {

/** This object is the in-memory representation of a song, as a collection of notes over time. */
class MusicSheet : public MusicDatabaseObject
{
public:
   /** Constructor */
   MusicSheet();

   /** Constructor */
   MusicSheet(const MusicSheet & rhs);

   /** Destructor */
   ~MusicSheet();
   
   /** Assignment operator */
   MusicSheet & operator = (const MusicSheet & rhs);

   /** Clears our state to the state of a just-constructed MusicSheet object */
   virtual void SetToDefaultState();

   /** Replaces this sheet's current contents with the contents from (archive) */
   virtual status_t SetFromArchive(const ConstMessageRef & archive);

   /** Saves this sheet's current contents into (archive) */
   virtual status_t SaveToArchive(const MessageRef & archive) const;

   /** Incrementally updates this sheet's contents, based on the information in the Message */
   status_t UpdateFromArchive(const ConstMessageRef & archive);

   /** Set the user-visible file-path string for this piece of music */
   void SetSongFilePath(const String & songFilePath);

   /** Returns the current user-visible file-path string for this piece of music */
   const String & GetSongFilePath() const {return _songFilePath;}

   /** Read-only access to our current table of chords */
   const OrderedKeysHashtable<uint32, uint64> & GetChordsTable() const {return _chords;}
  
   /** Convenience method -- returns the chord at the specified chord-index, or 0 if there are no notes at that index. */
   uint64 GetChordAtIndex(uint32 whichChord, bool useLoopingLogic) const {return _chords.GetWithDefault(useLoopingLogic?(whichChord%GetSongLengthInChords(false)):whichChord);}

   /** Places the specified chord into our chords-set.
     * @param whichChord The temporal index of the chord to set.
     * @param chordValue the notes to be present in the chord (0x00==silence, 0x01==lowest note, etc)
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
     */
   status_t PutChord(uint32 whichChord, uint64 chordValue);

   /** Convenience method; removes the given chord from the chords-set, if it exists.
     * This is equivalent to calling PutChord(whichChord, 0);
     * @param whichChord The temporal index of the chord to remove (0=first, 1=second, etc)
     */
   void RemoveChord(uint32 whichChord) {(void) PutChord(whichChord, 0);}

   /** Inserts space for a new chord at the given index.  Any other chords at or after that indexed are moved one column to the right. */
   void InsertChordAt(uint32 whichChord);

   /** Deletes the chord at the given index (if any).  Any other chords after that index are moved one column to the left. */
   void DeleteChordAt(uint32 whichChord);

   /** Returns the length of this song, in chords (i.e. the largest key in the chords Hashtable, plus one) */
   uint32 GetSongLengthInChords(bool useLoopingLogic) const {return ((useLoopingLogic)&&(_chords.HasItems())) ? MUSCLE_NO_LIMIT : (_chords.GetLastKeyWithDefault()+1);}

   /** Returns a bit-chord that describes the full set of all the notes that are currently used anywhere in this song. */
   uint64 GetAllUsedNotesChord() const {return _usedNotes;}

   /** Returns the checksum of this object (which is updated whenever this object's contents change) */
   virtual uint32 GetCurrentChecksum() const {return _checksum;}

   /** Calculates our current checksum from scratch (expensive!) */
   virtual uint32 CalculateChecksum() const;

   /** Updates our state as specified in the (seniorDoMsg).  Will only be called on the instance running on the senior peer. 
     * @param seniorDoMsg A Message containing instructions for how to update our state on the senior peer.
     * @returns a Message to send to the JuniorUpdate() method on the junior peers on success, or a NULL reference on failure.
     */
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);

   /** Updates our state as specified in the (juniorDoMsg).  Will only be called on the instance running on the senior peer.
     * @param juniorDoMsg A Message containing instructions for how to update our state on a junior peer.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);

   /** Returns a string representation of this object, for debugging purposes */
   virtual String ToString() const;

private:
   uint32 CalculateChecksumForChord(uint32 whichChord, uint64 chordValue) const {return ((whichChord+1)*CalculateChecksumForUint64(chordValue));}
   void MoveChordsBackOneStartingAt(uint32 whichChord);
   void SetToDefaultStateAux();

   String _songFilePath;                          // filepath this song was saved to (also used to generate a user-readable song title)
   OrderedKeysHashtable<uint32, uint64> _chords;  // time-index -> notes-chord (each bit indicates the presence or absence of a note)

   Hashtable<uint8, uint32> _noteHistogram;  // how many times each note is used in this song
   uint64 _usedNotes;                        // bit-chord of currently used notes (computed from the histogram)

   uint32 _checksum;    // always kept current, by updating it after each change
};
DECLARE_REFTYPES(MusicSheet);

}; // end namespace choir

#endif

#ifndef PlaybackState_h
#define PlaybackState_h

#include "MusicData.h"
#include "util/TimeUtilityFunctions.h"

namespace choir {

/** This object holds the state of how the currently-in-memory Music sheet should be performed over time */
class PlaybackState: public MusicDatabaseObject
{
public:
   /** Default constructor for an object that isn't going to be registered with a ZGDatabasePeerSession */
   PlaybackState();

   /** Constructor
     * @param session pointer to the ZGDatabasePeerSession object that created us
     * @param dbIndex the database index of this database in the ZGDatabasePeerSess
     */
   PlaybackState(ZGDatabasePeerSession * session, int32 dbIndex);

   /** Destructor */
   ~PlaybackState();

   /** Sets this object to its just-constructed state */
   virtual void SetToDefaultState();

   /** Replaces this sheet's current contents with the contents from (archive) */
   virtual status_t SetFromArchive(const ConstMessageRef & archive);

   /** Saves this sheet's current contents into (archive) */
   virtual status_t SaveToArchive(const MessageRef & archive) const;

   /** Just calls CalculateChecksum(), since this database is very small and thus CalculateChecksum() is still cheap */
   virtual uint32 GetCurrentChecksum() const {return CalculateChecksum();}

   /** Calculates and returns a checksum for this object */
   virtual uint32 CalculateChecksum() const;

   /** Updates our state as specified in the (seniorDoMsg).  Will only be called on the instance running on the senior peer.
     * @param seniorDoMsg A Message containing instructions for how to update our state on the senior peer.
     * @returns a Message to send to the JuniorUpdate() method on the junior peers on success, or a NULL reference on failure.
     */
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);

   /** Updates our state as specified in the (juniorDoMsg).  Will only be called on the instance running on the senior peer.
     * @param juniorDoMsg A Message containing instructions for how to update our state on a junior peer.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);

   /** Returns the time corresponding to the triggering of playback of the first note in the MusicSheet (or MUSCLE_TIME_NEVER if we shouldn't play) */
   uint64 GetNetworkStartTimeMicros() const {return _networkStartTimeMicros;}

   /** Returns the number of microseconds to be allotted to each chord in the MusicSheet */
   uint64 GetMicrosPerChord() const {return _microsPerChord;}

   /** If we're paused, returns the index of the chord that the playhead is currently paused at (or MUSCLE_NO_LIMIT if we aren't paused) */
   uint32 GetPausedIndex() const {return _pausedIndex;};

   /** Sets the time corresponding to the triggering of playback of the first note in the MusicSheet (or MUSCLE_TIME_NEVER if we shouldn't play) */
   void SetNetworkStartTimeMicros(uint64 networkStartTimeMicros) {_networkStartTimeMicros = networkStartTimeMicros;}

   /** Sets the number of microseconds to be allotted to each chord in the MusicSheet
     * @param microsPerChord the new microseconds-per-chord value to use (smaller == faster playback)
     * @param optNetworkNow If specified, and the state is currently set to "playing", we'll use this information to try to keep
     *                      the current-playback-point roughly constant despite the change in tempo.
     */
   void SetMicrosPerChord(uint64 microsPerChord, uint64 optNetworkNow = MUSCLE_TIME_NEVER);

   /** Sets returns the index of the chord that the playhead is currently paused at (or MUSCLE_NO_LIMIT if we don't want to be paused) */
   void SetPausedIndex(bool pausedIndex) {_pausedIndex = pausedIndex;};

   /** Convenience method -- returns true iff we are currently paused */
   bool IsPaused() const {return (_pausedIndex != MUSCLE_NO_LIMIT);}

   /** Set true if the music playback should loop; false if it should play through just once */
   void SetLoop(bool loop) {_loop = loop;}

   /** Returns true iff we should loop playback indefinitely (false means we should play only once) */
   bool IsLoop() const {return _loop;}

   /** Convenience method -- starts playback at the current seek position, if we're not already playing
     * @param networkNow The current network time (used to calculate the network-start-time-micros value)
     */
   void StartPlayback(uint64 networkNow);

   /** Convenience method -- pauses playback at the current seek position, if we're not already paused
     * @param networkNow The current network time (used to calculate the network-start-time-micros value)
     */
   void PausePlayback(uint64 networkNow);

   /** Convenience method -- seeks the current playback position to the specified chord
     * @param networkNow The current network time (used to calculate the network-start-time-micros value)
     * @param whichChord The chord to seek to (0 == start of song, 1 == second chord, etc)
     */
   void SeekTo(uint64 networkNow, uint32 whichChord);

   /** Convenience method:  Given a network time stamp, returns the chord-index that corresponds to that time
     * @param networkTimeStamp time-stamp to return a chord-index for
     * @param optLoopLengthChords if valid, we'll use this loop-length in our calculation.  If 0 or MUSCLE_TIME_NEVER, we won't.
     */
   uint32 GetChordIndexForNetworkTimeStamp(uint64 networkTimeStamp, uint32 optLoopLengthChords) const;

   /** Convenience method:  Given a chord index, returns the network-time at which that chord should be played. */
   uint64 GetNetworkTimeToPlayChord(uint32 chordIndex) const;

   /** Convenience method:  Given a network-timestamp, returns the corresponding offset from the top of the song (may be negative!) */
   int64 GetPlaybackPositionForNetworkTimeMicroseconds(uint64 networkTimestamp) const;

   /** Returns a string representation of this object, for debugging purposes */
   virtual String ToString() const;

private:
   void SetToDefaultStateAux();
   MessageRef SeniorAdjustPlaybackState(uint32 whatCode, const uint64 * optNewMicrosPerChord, const uint32 * optSeekTo, bool * optSetLoop);

   uint64 _networkStartTimeMicros; // the network-time at which chord #0 is/was to be played
   uint64 _microsPerChord;         // How many microseconds each chord represents (i.e. this controls the playback tempo)
   uint32 _pausedIndex;            // If playback is paused, this is the chord it's currently at; if MUSCLE_NO_LIMIT then we're playing
   bool _loop;                     // If true, then playback should loop forever; if false, we'll only play through once
};
DECLARE_REFTYPES(PlaybackState);

}; // end namespace choir

#endif

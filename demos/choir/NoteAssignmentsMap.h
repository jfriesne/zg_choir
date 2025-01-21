#ifndef NoteAssignmentsMap_h
#define NoteAssignmentsMap_h

#include "MusicData.h"

namespace choir {

/** This object specifies which peers should be playing which notes */
class NoteAssignmentsMap : public MusicDatabaseObject
{
public:
   /** Default constructor for an object that isn't going to be registered with a ZGDatabasePeerSession */
   NoteAssignmentsMap();

   /** Constructor
     * @param session pointer to the ZGDatabasePeerSession object that created us
     * @param dbIndex the database index of this database in the ZGDatabasePeerSess
     */
   NoteAssignmentsMap(ZGDatabasePeerSession * session, int32 dbIndex);

   /** Destructor */
   ~NoteAssignmentsMap();

   /** Sets this object to its just-constructed state */
   virtual void SetToDefaultState();

   /** Replaces this map's current contents with the contents from (archive) */
   virtual status_t SetFromArchive(const ConstMessageRef & archive);

   /** Saves this map's current contents into (archive) */
   virtual status_t SaveToArchive(const MessageRef & archive) const;

   /** For a given peer, returns the chord of notes that peer should play */
   const Hashtable<ZGPeerID, uint64> & GetNoteAssignments() const {return _noteAssignments;}

   /** Returns the note-assignments currently specified for the given peer. */
   uint64 GetNoteAssignmentsForPeerID(const ZGPeerID & peerID) const {return _noteAssignments[peerID];}

   /** Sets the notes-chord for the given peer ID to the given value */
   status_t SetNoteAssignmentsForPeerID(const ZGPeerID & peerID, uint64 newNotes);

   /** Un-assigns the specified note from everybody */
   void UnassignNote(uint32 noteIdx);

   /** Returns a bit-chord showing which notes are currently assigned to at least one peer */
   uint64 GetAllAssignedNotesChord() const {return _assignedNotes;}

   /** Set the assignment strategy we want the system to use for bell-assignments.
     * @param strategy a ASSIGNMENT_STRATEGY_* value
     */
   void SetAssignmentStrategy(uint32 strategy);

   /** Returns the assignment strategy the system is using for bell-assignments.  Default value is ASSIGNMENT_STRATEGY_AUTOMATIC. */
   uint32 GetAssignmentStrategy() const {return _assignmentStrategy;}

   /** Returns the checksum of this object (which is updated whenever this object's contents change) */
   virtual uint32 GetCurrentChecksum() const {return _checksum;}

   /** Recalculates our checksum from scratch (expensive!) */
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

   /** Updates this object based on the given CHOIR_COMMAND_TOGGLE_ASSIGNMENT Message's contents */
   status_t HandleToggleAssignmentMessage(const Message & msg);

   /** For debugging purposes */
   void Print(const OutputPrinter & p) const;

   /** Returns a string representation of this object, for debugging purposes */
   virtual String ToString() const;

   /** Returns a table that shows for each note currently in use, how many peers are
     * currently assigned to play that note.
     * (Keys are CHOIR_NOTE_* values, Values are assignment-counts, unassigned notes are not included in the table)
     */
   const Hashtable<uint8, uint32> & GetNoteHistogram() const {return _noteHistogram;}

private:
   uint32 CalculateChecksumForPeer(const ZGPeerID & peerID, uint64 chordValue) const {return peerID.CalculateChecksum()+CalculatePODChecksum(chordValue);}
   const ZGPeerID & GetLightestPeer(const Hashtable<ZGPeerID, ConstMessageRef> & onlinePeers, uint32 & retCount) const;
   const ZGPeerID & GetHeaviestPeer(const Hashtable<ZGPeerID, ConstMessageRef> & onlinePeers, uint32 & retCount) const;
   void SetToDefaultStateAux();
   void VerifyRunningChecksum(const char * desc) const; // just for debugging purposes
   status_t SeniorAutoUpdateAssignments(uint64 allNotesChord, bool & retChangedAnything);

   // checksummed data
   Hashtable<ZGPeerID, uint64> _noteAssignments;
   uint32 _assignmentStrategy;               // one of the ASSIGNMENT_STRATEGY_* values

   // metadata
   Hashtable<uint8, uint32> _noteHistogram;  // how many times each note is assigned to a peer
   uint64 _assignedNotes;                    // bit-chord of currently assigned notes (computed from the histogram)
   uint32 _checksum;                         // running checksum (so we don't have to recalculate it from scratch each time)
};
DECLARE_REFTYPES(NoteAssignmentsMap);

}; // end namespace choir

#endif

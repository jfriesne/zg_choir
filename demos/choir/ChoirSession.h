#ifndef ChoirSession_h
#define ChoirSession_h

#include "zg/ZGDatabasePeerSession.h"
#include "MusicData.h"

namespace choir {

class MusicSheet;
class PlaybackState;
class NoteAssignmentsMap;

/** This is the class that that the Choir demo uses to implement its data-replication functionality.  */
class ChoirSession : public ZGDatabasePeerSession
{
public:
   /** Constructor
     * @param peerSettings the ZGPeerSettings object to use for this system.
     */
   ChoirSession(const ZGPeerSettings & peerSettings);

   // PulseNode interface
   virtual uint64 GetPulseTime(const PulseArgs & args);
   virtual void Pulse(const PulseArgs & args);

   /** Sends the specified message to the GUI thread so the GUI will update its visible state to match ours.
     * @param msg Reference to a Message to send to the GUI
     * @param allowReviewTrigger if true, this call may also trigger a request for the senior peer to
     *                           auto-update the handbell-assignments map, if necessary
     * @param force If set to true, then this method will force the triggering of a review, no matter what.
     */
   void SendMessageToGUI(const ConstMessageRef & msg, bool allowReviewTrigger, bool force = false);

   /** Called to set what the results of the last notes-assignments review was.
     * This data will be used in subsequent calls to decide if another review is currently necessary, or not.
     * @param allNotesAtLastReview bit-chord of which notes were present in the song during the last review.
     */
   void SetReviewResults(uint64 allNotesAtLastReview);

protected:
   // ZGDatabasePeerSession interface
   IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase);

   // ZGPeerSession interface
   virtual void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo);
   virtual void PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo);
   virtual void SeniorPeerChanged(const ZGPeerID & oldSeniorPeerID, const ZGPeerID & newSeniorPeerID);

   /** Called when a Message object has been received from a fellow session on the same ReflectServer.
     * @param from The session object that is passing a Message to us
     * @param msg The Message object that (from) is passing to us
     * @param userData Currently unused
     */
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData);

private:
   bool IsStrategyReviewMaybeNecessary() const;
   status_t SendPeerOnlineOfflineMessageToGUI(uint32 whatCode, const ZGPeerID & id, const ConstMessageRef & optPeerInfo);
   ConstMessageRef GenerateLatenciesMessage() const;

   MusicSheet & GetMusicSheet();
   PlaybackState & GetPlaybackState();
   NoteAssignmentsMap & GetNoteAssignmentsMap();

   const MusicSheet & GetMusicSheet() const;
   const PlaybackState & GetPlaybackState() const;
   const NoteAssignmentsMap & GetNoteAssignmentsMap() const;

   bool _requestAssignmentStrategyReviewPending;
   uint64 _allNotesAtLastReview;
   Hashtable<ZGPeerID, ConstMessageRef> _peersAtLastReview;

   uint64 _nextSendLatenciesTime;
};
DECLARE_REFTYPES(ChoirSession);

}; // end namespace choir

#endif

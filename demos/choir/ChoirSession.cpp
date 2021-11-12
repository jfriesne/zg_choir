#include "system/MessageTransceiverThread.h"

#include "ChoirProtocol.h"
#include "ChoirSession.h"
#include "MusicSheet.h"
#include "PlaybackState.h"
#include "NoteAssignmentsMap.h"

namespace choir {

ChoirSession :: ChoirSession(const ZGPeerSettings & peerSettings) : ZGDatabasePeerSession(peerSettings), _requestAssignmentStrategyReviewPending(false), _allNotesAtLastReview(0), _nextSendLatenciesTime(0)
{
   // empty
}

uint64 ChoirSession :: GetPulseTime(const PulseArgs & args)
{
   if (_requestAssignmentStrategyReviewPending) return 0;  // we want to do a review ASAP!
   return muscleMin(ZGDatabasePeerSession::GetPulseTime(args), _nextSendLatenciesTime);
}

void ChoirSession :: Pulse(const PulseArgs & args)
{
   ZGDatabasePeerSession::Pulse(args);

   if (_requestAssignmentStrategyReviewPending)
   {
      _requestAssignmentStrategyReviewPending = false;

      // We need to do the review as a database update since it might change the assignments map!
      if ((IAmTheSeniorPeer())&&(GetNoteAssignmentsMap().GetAssignmentStrategy() != ASSIGNMENT_STRATEGY_MANUAL))
      {
         MessageRef reqMsg = GetMessageFromPool(CHOIR_COMMAND_REVIEW_ASSIGNMENTS);
         if (reqMsg()) (void) RequestUpdateDatabaseState(CHOIR_DATABASE_ROSTER, reqMsg);
      }
   }

   if (args.GetCallbackTime() >= _nextSendLatenciesTime)
   {
      SendMessageToGUI(GenerateLatenciesMessage(), false);
      _nextSendLatenciesTime = args.GetCallbackTime() + MillisToMicros(500);
   }
}

ConstMessageRef ChoirSession :: GenerateLatenciesMessage() const
{
   MessageRef msg = GetMessageFromPool(CHOIR_REPLY_LATENCIES_TABLE);
   if (msg() == NULL) return ConstMessageRef();

   for (HashtableIterator<ZGPeerID, ConstMessageRef> iter(GetOnlinePeers()); iter.HasData(); iter++)
   {
      const ZGPeerID & pid = iter.GetKey();
      if ((msg()->AddFlat(CHOIR_NAME_PEER_ID, pid).IsOK())&&(msg()->AddInt64(CHOIR_NAME_PEER_LATENCY, GetEstimatedLatencyToPeer(pid)).IsError())) 
      {
         (void) msg()->RemoveData(CHOIR_NAME_PEER_ID, msg()->GetNumValuesInName(CHOIR_NAME_PEER_ID)-1);  // roll back!
      }
   }
   return AddConstToRef(msg);
}

bool ChoirSession :: IsStrategyReviewMaybeNecessary() const
{
   // If the strategy is manual, then we never need to do anything
   if (GetNoteAssignmentsMap().GetAssignmentStrategy() == ASSIGNMENT_STRATEGY_MANUAL) return false;  // duh

   // If we have unassigned notes, we'd better review so we can try to assign them
   const uint64 allNotesInMusic = GetMusicSheet().GetAllUsedNotesChord();
   const uint64 orphanNotesChord = allNotesInMusic & ~GetNoteAssignmentsMap().GetAllAssignedNotesChord();
   if (orphanNotesChord != 0) return true;

   if (GetNoteAssignmentsMap().GetAssignmentStrategy() == ASSIGNMENT_STRATEGY_AUTOMATIC)
   {
      // In full automatic mode, we may want to rebalance whenever new notes or new peers are introduced
      const uint64 newNotes = allNotesInMusic & ~_allNotesAtLastReview;
      if (newNotes != 0) return true;

      if (GetOnlinePeers().IsEqualTo(_peersAtLastReview) == false) return true;

      // Finally, in automatic mode if we see any notes assigned to more than one peer we'll want to unassign them
      for (HashtableIterator<uint8, uint32> iter(GetNoteAssignmentsMap().GetNoteHistogram()); iter.HasData(); iter++) if (iter.GetValue() > 1) return true;
   }
   return false;
}

void ChoirSession :: SendMessageToGUI(const ConstMessageRef & msg, bool allowReviewTrigger, bool force)
{
   // We'll send this Message to the supervisor session, and he'll send it back to the GUI thread
   ThreadSupervisorSession * supervisorSession = FindFirstSessionOfType<ThreadSupervisorSession>();
   if (supervisorSession)
   {
      MessageRef wrapper = GetMessageFromPool(CHOIR_REPLY_GUI_UPDATE);
      if ((wrapper())&&(wrapper()->AddMessage(CHOIR_NAME_WRAPPED_MESSAGE, CastAwayConstFromRef(msg)).IsOK())) supervisorSession->MessageReceivedFromSession(*this, wrapper, NULL);

      if ((allowReviewTrigger)&&(IAmTheSeniorPeer())&&(_requestAssignmentStrategyReviewPending == false)&&((force)||(IsStrategyReviewMaybeNecessary())))
      {
         _requestAssignmentStrategyReviewPending = true;
         InvalidatePulseTime();
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "SendMessageToGUI:  Couldn't find supervisor session!\n");
}

void ChoirSession :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData)
{
   switch(msgRef()->what)
   {
      case MTT_COMMAND_SEND_USER_MESSAGE:
      {
         // unwrap the user Message if we have to
         MessageRef subMsg = msgRef()->GetMessage(MTT_NAME_MESSAGE);
         if (subMsg()) MessageReceivedFromSession(from, subMsg, userData);
      }
      break;

      case CHOIR_COMMAND_SET_SONG_FILE_PATH:
         if (RequestUpdateDatabaseState(CHOIR_DATABASE_SCORE, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send song-file-path-update request to senior peer!\n");
      break;

      case CHOIR_COMMAND_TOGGLE_NOTE: case CHOIR_COMMAND_INSERT_CHORD: case CHOIR_COMMAND_DELETE_CHORD:
         // This message is from our local GUI, telling us that the user clicked on the MusicSheetWidget
         if (RequestUpdateDatabaseState(CHOIR_DATABASE_SCORE, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send note-update request to senior peer!\n");
      break;

      case CHOIR_COMMAND_PLAY: case CHOIR_COMMAND_PAUSE: case CHOIR_COMMAND_ADJUST_PLAYBACK:
         // This message is from our local GUI, telling us that the user wants to modify the playback state
         if (RequestUpdateDatabaseState(CHOIR_DATABASE_PLAYBACKSTATE, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send playback-state-update request to senior peer!\n");
      break;

      case CHOIR_COMMAND_TOGGLE_ASSIGNMENT: case CHOIR_COMMAND_SET_STRATEGY:
         // This message is from our local GUI, telling us that the user clicked on the RosterWidget
         if (RequestUpdateDatabaseState(CHOIR_DATABASE_ROSTER, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send note-assignment request to senior peer!\n");
      break;

      case MUSIC_TYPE_MUSIC_SHEET:
         // This message is from our local GUI, telling us that the user wants to fully replace the score database
         if (RequestReplaceDatabaseState(CHOIR_DATABASE_SCORE, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send score-replace request to senior peer!\n");
      break;

      case MUSIC_TYPE_PLAYBACK_STATE:
         // This message is from our local GUI, telling us that the user wants to fully replace the playback-state database
         if (RequestReplaceDatabaseState(CHOIR_DATABASE_PLAYBACKSTATE, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send playback-state-replace request to senior peer!\n");
      break;

      case MUSIC_TYPE_ASSIGNMENTS_MAP:
         // This message is from our local GUI, telling us that the user wants to fully replace the note-assignments database
         if (RequestReplaceDatabaseState(CHOIR_DATABASE_ROSTER, msgRef).IsError()) LogTime(MUSCLE_LOG_ERROR, "Couldn't send assigns-state-replace request to senior peer!\n");
      break;

      default:
         ZGDatabasePeerSession::MessageReceivedFromSession(from, msgRef, userData);
      break;
   }
}

status_t ChoirSession :: SendPeerOnlineOfflineMessageToGUI(uint32 whatCode, const ZGPeerID & id, const ConstMessageRef & optPeerInfo)
{
   MessageRef msg = GetMessageFromPool(whatCode);
   MRETURN_OOM_ON_NULL(msg());

   MRETURN_ON_ERROR(msg()->AddFlat(CHOIR_NAME_PEER_ID, id));
   MRETURN_ON_ERROR(msg()->CAddMessage(CHOIR_NAME_PEER_INFO, CastAwayConstFromRef(optPeerInfo)));
   SendMessageToGUI(msg, true);
   return B_NO_ERROR;
}

void ChoirSession :: PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   ZGDatabasePeerSession::PeerHasComeOnline(peerID, peerInfo);
   (void) SendPeerOnlineOfflineMessageToGUI(CHOIR_COMMAND_PEER_ONLINE, peerID, peerInfo);
}

void ChoirSession :: PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & peerInfo)
{
   ZGDatabasePeerSession::PeerHasGoneOffline(peerID, peerInfo);
   (void) SendPeerOnlineOfflineMessageToGUI(CHOIR_COMMAND_PEER_OFFLINE, peerID, peerInfo);

   // If that peer had any notes assigned to him, we need to un-assign those notes
   if (IAmTheSeniorPeer()) (void) RequestUpdateDatabaseState(CHOIR_DATABASE_ROSTER, GetMessageFromPool(CHOIR_COMMAND_UNASSIGN_ORPHANS));
}

void ChoirSession :: SeniorPeerChanged(const ZGPeerID & oldSeniorPeerID, const ZGPeerID & newSeniorPeerID)
{
   ZGDatabasePeerSession::SeniorPeerChanged(oldSeniorPeerID, newSeniorPeerID);

   if (IAmTheSeniorPeer())
   {
      // Now that I'm in charge, let's make sure there are no orphaned notes around
      _peersAtLastReview.Clear();
      _allNotesAtLastReview = 0;
   }

   MessageRef msg = GetMessageFromPool(CHOIR_REPLY_NEW_SENIOR_PEER);
   if ((msg())&&(msg()->AddFlat(CHOIR_NAME_PEER_ID, newSeniorPeerID).IsOK())) SendMessageToGUI(msg, true, true);
}

void ChoirSession :: SetReviewResults(uint64 allNotesAtLastReview)
{
   _allNotesAtLastReview = allNotesAtLastReview;
   _peersAtLastReview    = GetOnlinePeers();
}

MusicSheet & ChoirSession :: GetMusicSheet()
{
   return *static_cast<MusicSheet *>(GetDatabaseObject(CHOIR_DATABASE_SCORE));
}

PlaybackState & ChoirSession :: GetPlaybackState()
{
   return *static_cast<PlaybackState *>(GetDatabaseObject(CHOIR_DATABASE_PLAYBACKSTATE));
}

NoteAssignmentsMap & ChoirSession :: GetNoteAssignmentsMap()
{
   return *static_cast<NoteAssignmentsMap *>(GetDatabaseObject(CHOIR_DATABASE_ROSTER));
}

const MusicSheet & ChoirSession :: GetMusicSheet() const
{
   return *static_cast<const MusicSheet *>(GetDatabaseObject(CHOIR_DATABASE_SCORE));
}

const PlaybackState & ChoirSession :: GetPlaybackState() const
{
   return *static_cast<const PlaybackState *>(GetDatabaseObject(CHOIR_DATABASE_PLAYBACKSTATE));
}

const NoteAssignmentsMap & ChoirSession :: GetNoteAssignmentsMap() const
{
   return *static_cast<const NoteAssignmentsMap *>(GetDatabaseObject(CHOIR_DATABASE_ROSTER));
}

IDatabaseObjectRef ChoirSession :: CreateDatabaseObject(uint32 whichDatabase)
{
   IDatabaseObjectRef ret;
   switch(whichDatabase)
   {
      case CHOIR_DATABASE_SCORE:         ret.SetRef(newnothrow MusicSheet(        this, whichDatabase)); break;
      case CHOIR_DATABASE_PLAYBACKSTATE: ret.SetRef(newnothrow PlaybackState(     this, whichDatabase)); break;
      case CHOIR_DATABASE_ROSTER:        ret.SetRef(newnothrow NoteAssignmentsMap(this, whichDatabase)); break;
      default:                           /* empty */                                                     break;
   }
   if (ret() == NULL) MWARN_OUT_OF_MEMORY;
   return ret;
}

}; // end namespace choir

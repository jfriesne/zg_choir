#include "ChoirProtocol.h"
#include "MusicSheet.h"
#include "NoteAssignmentsMap.h"
#include "PlaybackState.h"

namespace choir {

NoteAssignmentsMap :: NoteAssignmentsMap()
{
   SetToDefaultStateAux();
}

NoteAssignmentsMap :: NoteAssignmentsMap(ZGDatabasePeerSession * session, int32 dbID) : MusicDatabaseObject(session, dbID)
{
   SetToDefaultStateAux();
}

NoteAssignmentsMap :: ~NoteAssignmentsMap()
{
   // empty
}

void NoteAssignmentsMap :: SetToDefaultState()
{
   SetToDefaultStateAux();
   (void) SendFullStateToGUI(true);
}

void NoteAssignmentsMap :: SetToDefaultStateAux()
{
   _noteAssignments.Clear();
   _noteHistogram.Clear();
   _assignedNotes      = 0;
   _assignmentStrategy = ASSIGNMENT_STRATEGY_AUTOMATIC;
   _checksum           = CalculateChecksum();
}

status_t NoteAssignmentsMap :: SetFromArchive(const ConstMessageRef & archiveRef)
{
   const Message & archive = *archiveRef();
   if (archive.what != MUSIC_TYPE_ASSIGNMENTS_MAP) return B_TYPE_MISMATCH;

   SetToDefaultStateAux();

   SetAssignmentStrategy(archive.GetInt32("strategy"));

   ZGPeerID tempID;
   uint64 tempChord;
   for (int32 i=0; ((archive.FindFlat("pid", i, tempID).IsOK())&&(archive.FindInt64("chord", i, tempChord).IsOK())); i++)
      MRETURN_ON_ERROR(SetNoteAssignmentsForPeerID(tempID, tempChord));

   SendMessageToGUI(archiveRef, true);
   return B_NO_ERROR;
}

status_t NoteAssignmentsMap :: SaveToArchive(const MessageRef & archiveRef) const
{
   Message & archive = *archiveRef();
   archive.what = MUSIC_TYPE_ASSIGNMENTS_MAP;

   for (HashtableIterator<ZGPeerID, uint64> iter(_noteAssignments); iter.HasData(); iter++)
   {
      MRETURN_ON_ERROR(archive.AddFlat("pid", iter.GetKey()));
      MRETURN_ON_ERROR(archive.AddInt64("chord", iter.GetValue()));
   }
   return archive.AddInt32("strategy", _assignmentStrategy);
}

// This is just here for debugging -- in actual use it shouldn't be necessary since we keep
// a running checksum instead (but the running checksum should always be equal to the value returned
// by this method!)
uint32 NoteAssignmentsMap :: CalculateChecksum() const
{
   uint32 ret = _assignmentStrategy;
   for (HashtableIterator<ZGPeerID, uint64> iter(_noteAssignments); iter.HasData(); iter++) ret += CalculateChecksumForPeer(iter.GetKey(), iter.GetValue());
   return ret;
}

void NoteAssignmentsMap :: VerifyRunningChecksum(const char * desc) const
{
   uint32 cc = CalculateChecksum();
   if (cc != _checksum)
   {
      LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap(%s):  Checksum verification failed!  Running checksum is " UINT32_FORMAT_SPEC ", should have been " UINT32_FORMAT_SPEC "\n", desc, _checksum, cc);
   }
}

void NoteAssignmentsMap :: SetAssignmentStrategy(uint32 strategy) 
{
   _checksum          -= _assignmentStrategy; 
   _assignmentStrategy = strategy; 
   _checksum          += _assignmentStrategy;
}

status_t NoteAssignmentsMap :: SetNoteAssignmentsForPeerID(const ZGPeerID & peerID, uint64 assignedNotes)
{
   if (assignedNotes != 0)
   {
      uint64 * val = _noteAssignments.GetOrPut(peerID);
      MRETURN_OOM_ON_NULL(val);  // out of memory?

      if (*val) 
      {
         UpdateNoteHistogram(*val, false, _noteHistogram, _assignedNotes);
         _checksum -= CalculateChecksumForPeer(peerID, *val);
      }
      *val = assignedNotes;
      if (*val) 
      {
         UpdateNoteHistogram(*val, true, _noteHistogram, _assignedNotes);
         _checksum += CalculateChecksumForPeer(peerID, *val);
      }

      return B_NO_ERROR;
   }
   else
   {
      const uint64 * oldVal = _noteAssignments.Get(peerID);
      if (oldVal)
      {
         UpdateNoteHistogram(*oldVal, false, _noteHistogram, _assignedNotes);
         _checksum -= CalculateChecksumForPeer(peerID, *oldVal);
         (void) _noteAssignments.Remove(peerID);  // must be done last, as this invalidates (peerID)!
      }
      return B_NO_ERROR;  // if (peerID) isn't present anymore, that's good enough for us to declare success
   }
}

void NoteAssignmentsMap :: UnassignNote(uint32 noteIdx)
{
   for (HashtableIterator<ZGPeerID, uint64> iter(_noteAssignments); iter.HasData(); iter++)
   {
      const uint64 chord = iter.GetValue();
      if ((chord & (1LL<<noteIdx)) != 0) (void) SetNoteAssignmentsForPeerID(iter.GetKey(), chord & ~(1LL<<noteIdx));
   }
}

// Utility code to handle the CHOIR_COMMAND_TOGGLE_ASSIGNMENT Message, which can be handled the same way on both senior and junior peers
status_t NoteAssignmentsMap :: HandleToggleAssignmentMessage(const Message & msg)
{
   if (msg.what != CHOIR_COMMAND_TOGGLE_ASSIGNMENT)
   {
      LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap::HandleToggleAssignmentMessage:  wrong Message type " UINT32_FORMAT_SPEC "\n", msg.what);
      return B_TYPE_MISMATCH;
   }

   status_t ret;
   ZGPeerID peerID;
   if (msg.FindFlat(CHOIR_NAME_PEER_ID, peerID).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap::HandleToggleAssignmentMessage:  no CHOIR_NAME_PEER_ID found!\n");
      return ret;
   }

   const uint32 noteIdx = msg.GetInt32(CHOIR_NAME_NOTE_INDEX);

        if (peerID.IsValid())  return SetNoteAssignmentsForPeerID(peerID, GetNoteAssignmentsForPeerID(peerID)^(1LL<<noteIdx));
   else if (_assignedNotes!=0) UnassignNote(noteIdx);

   return B_NO_ERROR;
}

static uint32 CountBits(uint64 chord)
{
   uint32 ret = 0;
   for (uint32 i=0; i<NUM_CHOIR_NOTES; i++) if (chord&(1LL<<i)) ret++;
   return ret;
}

static uint32 GetLastNoteInChord(uint64 chord)
{
   for (int32 i=NUM_CHOIR_NOTES-1; i>=0; i--) if ((chord & (1LL<<i)) != 0) return i;
   return 0;  // I guess?
}

const ZGPeerID & NoteAssignmentsMap :: GetLightestPeer(const Hashtable<ZGPeerID, ConstMessageRef> & onlinePeers, uint32 & retCount) const
{
   retCount = MUSCLE_NO_LIMIT;
   const ZGPeerID * ret = onlinePeers.GetFirstKey();

   for (HashtableIterator<ZGPeerID, ConstMessageRef> iter(onlinePeers); iter.HasData(); iter++)
   {
      const ZGPeerID & pid = iter.GetKey();
      const uint32 count = CountBits(GetNoteAssignmentsForPeerID(pid));
      if (count < retCount)
      {
         retCount = count;
         ret      = &pid;
      }
   }
   return *ret;
}

const ZGPeerID & NoteAssignmentsMap :: GetHeaviestPeer(const Hashtable<ZGPeerID, ConstMessageRef> & onlinePeers, uint32 & retCount) const
{
   retCount = 0;
   const ZGPeerID * ret = onlinePeers.GetFirstKey();

   for (HashtableIterator<ZGPeerID, ConstMessageRef> iter(onlinePeers); iter.HasData(); iter++)
   {
      const ZGPeerID & pid = iter.GetKey();
      const uint32 count = CountBits(GetNoteAssignmentsForPeerID(pid));
      if (count > retCount)
      {
         retCount = count;
         ret      = &pid;
      }
   }
   return *ret;
}

status_t NoteAssignmentsMap :: SeniorAutoUpdateAssignments(uint64 allNotesChord, bool & retChangedAnything)
{
   retChangedAnything = false;
   if (_assignmentStrategy == ASSIGNMENT_STRATEGY_MANUAL) return B_NO_ERROR;  // in manual mode, we don't touch anything

   // For either of the two other modes, the first thing to do is forget about any peers who aren't online anymore
   const Hashtable<ZGPeerID, ConstMessageRef> & onlinePeers = GetOnlinePeers();
   for (HashtableIterator<ZGPeerID, uint64> iter(_noteAssignments); iter.HasData(); iter--) 
   {
      const ZGPeerID & pid = iter.GetKey();
      if (onlinePeers.ContainsKey(pid) == false)
      {
         MRETURN_ON_ERROR(SetNoteAssignmentsForPeerID(pid, 0));
         retChangedAnything = true;
      }
   }

   // If there are no online peers, we're done here (I don't think this would ever happen but I'm paranoid)
   if (onlinePeers.IsEmpty()) return B_NO_ERROR;

   // Then look for any notes that are orphaned, and assign them to whichever peer has the least notes currently assigned
   for (uint32 i=0; i<NUM_CHOIR_NOTES; i++)
   {
      if (((allNotesChord & (1LL<<i)) != 0)&&((_assignedNotes & (1LL<<i)) == 0))
      {
         uint32 lightCount = 0;
         const ZGPeerID & pid = GetLightestPeer(onlinePeers, lightCount);
         MRETURN_ON_ERROR(SetNoteAssignmentsForPeerID(pid, GetNoteAssignmentsForPeerID(pid)|(1LL<<i)));
         retChangedAnything = true;
      }
   }

   // In assisted mode, that's all we do, but in full automatic we'll also do load balancing
   if (_assignmentStrategy == ASSIGNMENT_STRATEGY_ASSISTED) return B_NO_ERROR;

   // Look for any notes that are assigned to more than one peer; when we find them we'll remove the redundant notes
   for (HashtableIterator<uint8, uint32> histIter(_noteHistogram); histIter.HasData(); histIter++)
   {
      if (histIter.GetValue() > 1)  // ooh, a multi-assigned note!
      {
         const uint64 noteBit = (1LL << histIter.GetKey());
         bool foundNoteAlready = false;

         for (HashtableIterator<ZGPeerID, uint64> iter(_noteAssignments); iter.HasData(); iter++)
         {
            const ZGPeerID & pid = iter.GetKey();
            const uint64 peerNotes = iter.GetValue();
            if (peerNotes & noteBit)
            {
               if (foundNoteAlready == false) foundNoteAlready = true;
               else 
               {
                  MRETURN_ON_ERROR(SetNoteAssignmentsForPeerID(pid, GetNoteAssignmentsForPeerID(pid)&~noteBit));
                  retChangedAnything = true;
               }
            } 
         }
      }
   }

   // And finally, for as long as the most-heavily-loaded peer has two or more notes than the
   // most-lightly-loaded peer, we'll move a note from Mr. Heavy to Mr. Light.
   if (onlinePeers.GetNumItems() > 1)
   {
      while(true)
      {
         uint32 lightCount, heavyCount; 
         const ZGPeerID & mrLight = GetLightestPeer(onlinePeers, lightCount);
         const ZGPeerID & mrHeavy = GetHeaviestPeer(onlinePeers, heavyCount);
         if (muscleAbs((int32)(heavyCount-lightCount)) > 1)
         {
            const uint32 noteIdx = GetLastNoteInChord(GetNoteAssignmentsForPeerID(mrHeavy));
            MRETURN_ON_ERROR(SetNoteAssignmentsForPeerID(mrHeavy, GetNoteAssignmentsForPeerID(mrHeavy)&~(1LL<<noteIdx)));
            MRETURN_ON_ERROR(SetNoteAssignmentsForPeerID(mrLight, GetNoteAssignmentsForPeerID(mrLight)| (1LL<<noteIdx)));
            retChangedAnything = true; 
         }
         else break;
      }
   }

   return B_NO_ERROR;
}

void NoteAssignmentsMap :: PrintToStream() const
{
   printf("%s\n", ToString()());
}

String NoteAssignmentsMap :: ToString() const
{
   String ret;
   char buf[512]; muscleSprintf(buf, "NoteAssignmentsMap %p has " UINT32_FORMAT_SPEC " entries, _assignedNotes is " UINT64_FORMAT_SPEC ", _assignmentStrategy is " UINT32_FORMAT_SPEC ", _checksum is " UINT32_FORMAT_SPEC "\n", this, _noteAssignments.GetNumItems(), _assignedNotes, _assignmentStrategy, _checksum);
   ret = buf;

   for (HashtableIterator<ZGPeerID, uint64> iter(_noteAssignments); iter.HasData(); iter++) 
   {
      muscleSprintf(buf, "   [%s] -> " XINT64_FORMAT_SPEC "\n", iter.GetKey().ToString()(), iter.GetValue());
      ret += buf;
   }
   return ret;
}

ConstMessageRef NoteAssignmentsMap :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   switch(seniorDoMsg()->what)
   {
      case CHOIR_COMMAND_TOGGLE_ASSIGNMENT:
      {
         // Junior peers can just handle the same command the same way and get the same result
         if (HandleToggleAssignmentMessage(*seniorDoMsg()).IsOK()) 
         {
            SendMessageToGUI(seniorDoMsg, true);
            return seniorDoMsg;
         }
      }
      break;

      case CHOIR_COMMAND_UNASSIGN_ORPHANS:
      {
         bool unassignedAny = false;
         for (HashtableIterator<ZGPeerID, uint64> iter(GetNoteAssignments()); iter.HasData(); iter++)
         {
            const ZGPeerID & peerID = iter.GetKey();
            if (IsPeerOnline(peerID) == false)
            {
               (void) SetNoteAssignmentsForPeerID(peerID, 0);  // no more notes for you!
               unassignedAny = true;
            }
         }

         if (unassignedAny) return SendFullStateToGUI(true);
                       else return GetMessageFromPool(CHOIR_COMMAND_NOOP);  // nothing for the junior peers to do!
      }
      break;

      case CHOIR_COMMAND_NOOP:
         return seniorDoMsg;

      case CHOIR_COMMAND_REVIEW_ASSIGNMENTS:
      {
         const MusicSheet * musicSheet = static_cast<const MusicSheet *>(GetDatabaseObject(CHOIR_DATABASE_SCORE));
         if (musicSheet == NULL)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "NoteAssignmentsMap::SeniorUpdate():  MusicSheet not available, perhaps we're not getting called from the right context?\n");
            return MessageRef();
         }

         bool changedAnything = false;
         if (SeniorAutoUpdateAssignments(musicSheet->GetAllUsedNotesChord(), changedAnything).IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "Strategy review failed!\n");
            return MessageRef();
         }

         SetReviewResults(musicSheet->GetAllUsedNotesChord());

         if (changedAnything) return SendFullStateToGUI(false);
                         else return GetMessageFromPool(CHOIR_COMMAND_NOOP);  // nothing for the junior peers to do!
      }
      break;

      case CHOIR_COMMAND_SET_STRATEGY:
         SetAssignmentStrategy(seniorDoMsg()->GetInt32(CHOIR_NAME_STRATEGY));
         SendMessageToGUI(seniorDoMsg, true);
         return seniorDoMsg;

      default:
         LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap::SeniorUpdate():  Unknown message code " UINT32_FORMAT_SPEC "\n", seniorDoMsg()->what);
      break;
   }

   LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap::SeniorUpdate() failed!\n");
   return MessageRef();
}

status_t NoteAssignmentsMap :: JuniorUpdate(const ConstMessageRef & juniorDoMsg)
{
   switch(juniorDoMsg()->what)
   {
      case CHOIR_COMMAND_NOOP:
         return B_NO_ERROR;

      case CHOIR_COMMAND_TOGGLE_ASSIGNMENT:
         MRETURN_ON_ERROR(HandleToggleAssignmentMessage(*juniorDoMsg()));
         SendMessageToGUI(juniorDoMsg, true);
         return B_NO_ERROR;

      case CHOIR_COMMAND_SET_STRATEGY:
         SetAssignmentStrategy(juniorDoMsg()->GetInt32(CHOIR_NAME_STRATEGY));
         SendMessageToGUI(juniorDoMsg, true);
         return B_NO_ERROR;

      case MUSIC_TYPE_ASSIGNMENTS_MAP:
         return SetFromArchive(juniorDoMsg);

      default:
         LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap::JuniorUpdate():  Unknown message code " UINT32_FORMAT_SPEC "\n", juniorDoMsg()->what);
      break;
   }

   LogTime(MUSCLE_LOG_ERROR, "NoteAssignmentsMap::JuniorUpdate() failed!\n");
   return B_BAD_ARGUMENT;
}

}; // end namespace choir

#include "MusicSheet.h"
#include "ChoirProtocol.h"

namespace choir {

MusicSheet :: MusicSheet()
{
   SetToDefaultStateAux();
}

MusicSheet :: MusicSheet(ZGDatabasePeerSession * session, int32 dbID) : MusicDatabaseObject(session, dbID)
{
   SetToDefaultStateAux();
}

MusicSheet :: MusicSheet(const MusicSheet & rhs) : MusicDatabaseObject(rhs.GetDatabasePeerSession(), rhs.GetDatabaseIndex())
{
   *this = rhs;
}

MusicSheet :: ~MusicSheet()
{
   // empty
}

MusicSheet & MusicSheet :: operator = (const MusicSheet & rhs)
{
   SetToDefaultStateAux();
   SetSongFilePath(rhs.GetSongFilePath());
   (void) _chords.EnsureSize(rhs._chords.GetNumItems());
   for (HashtableIterator<uint32, uint64> iter(rhs._chords); iter.HasData(); iter++) (void) PutChord(iter.GetKey(), iter.GetValue());
   return *this;
}

void MusicSheet :: SetSongFilePath(const String & songFilePath)
{
   if (songFilePath != _songFilePath)
   {
      _checksum -= _songFilePath.CalculateChecksum();
      _songFilePath = songFilePath;
      _checksum += _songFilePath.CalculateChecksum();
   }
}

uint32 MusicSheet :: CalculateChecksum() const
{
   uint32 ret = _songFilePath.CalculateChecksum();
   for (HashtableIterator<uint32, uint64> iter(_chords); iter.HasData(); iter++) ret += CalculateChecksumForChord(iter.GetKey(), iter.GetValue());
   return ret;
}

void MusicSheet :: SetToDefaultState()
{
   SetToDefaultStateAux();
   (void) SendFullStateToGUI(true);
}

void MusicSheet :: SetToDefaultStateAux()
{
   _songFilePath.Clear();
   _chords.Clear();
   _noteHistogram.Clear();
   _usedNotes = 0;
   _checksum  = 0;
}

status_t MusicSheet :: SetFromArchive(const ConstMessageRef & archiveRef)
{
   const Message & archive = *archiveRef();
   if (archive.what != MUSIC_TYPE_MUSIC_SHEET) return B_TYPE_MISMATCH;

   SetToDefaultStateAux();
   MRETURN_ON_ERROR(UpdateFromArchive(archiveRef));
   SendMessageToGUI(archiveRef, true);
   return B_NO_ERROR;
}

status_t MusicSheet :: UpdateFromArchive(const ConstMessageRef & archiveRef)
{
   const Message & archive = *archiveRef();
   if (archive.what != MUSIC_TYPE_MUSIC_SHEET) return B_TYPE_MISMATCH;

   const String * newSongFilePath;
   if (archive.FindString("path", &newSongFilePath).IsOK()) SetSongFilePath(*newSongFilePath);

   uint32 nextIndex;
   uint64 nextChord;
   for (uint32 i=0; ((archive.FindInt32("idx", i, nextIndex).IsOK())&&(archive.FindInt64("chord", i, nextChord).IsOK())); i++) MRETURN_ON_ERROR(PutChord(nextIndex, nextChord));

   return B_NO_ERROR;
}

status_t MusicSheet :: SaveToArchive(const MessageRef & archiveRef) const
{
   Message & archive = *archiveRef();
   archive.what = MUSIC_TYPE_MUSIC_SHEET;

   MRETURN_ON_ERROR(archive.AddString("path", _songFilePath));

   for (HashtableIterator<uint32, uint64> iter(_chords); iter.HasData(); iter++)
   {
      MRETURN_ON_ERROR(archive.AddInt32("idx", iter.GetKey()));
      MRETURN_ON_ERROR(archive.AddInt64("chord", iter.GetValue()));
   }

   return B_NO_ERROR;
}

status_t MusicSheet :: PutChord(uint32 whichChord, uint64 chordValue)
{
   if (chordValue != 0)
   {
      uint64 * val = _chords.GetOrPut(whichChord);
      MRETURN_ON_NULL(val); // out of memory?

      if (*val) 
      {
         UpdateNoteHistogram(*val, false, _noteHistogram, _usedNotes);
         _checksum -= CalculateChecksumForChord(whichChord, *val);
      }
      *val = chordValue;
      if (*val) 
      {
         UpdateNoteHistogram(*val, true, _noteHistogram, _usedNotes);
         _checksum += CalculateChecksumForChord(whichChord, *val);
      }

      return B_NO_ERROR;
   }
   else
   {
      uint64 oldVal = 0;
      if (_chords.Remove(whichChord, oldVal).IsOK()) 
      {
         UpdateNoteHistogram(oldVal, false, _noteHistogram, _usedNotes);
         _checksum -= CalculateChecksumForChord(whichChord, oldVal);
      }
      return B_NO_ERROR;  // if (whichChord) isn't present anymore, that's good enough for us to declare success
   }
}

void MusicSheet :: InsertChordAt(uint32 whichChord)
{
   // Iterate backwards until we get to the first chord index before (whichChord)
   for (HashtableIterator<uint32, uint64> iter(_chords, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
   {
      const uint32 chordIdx = iter.GetKey();
      if (chordIdx < whichChord) break;  // done!

      const uint64 chordValue = iter.GetValue();  // gotta copy this out since RemoveChord() will invalidate iter.GetValue()
      (void) RemoveChord(chordIdx);               // guaranteed to succeed
      (void) PutChord(chordIdx+1, chordValue);    // guaranteed to succeed
   }
}

void MusicSheet :: DeleteChordAt(uint32 whichChord)
{
   (void) RemoveChord(whichChord);

   if (_chords.HasItems())
   {
      if (whichChord > *_chords.GetLastKey()) return;  // nothing to do here!

      // Iterate backwards until we get to the first chord index before (whichChord)
      for (HashtableIterator<uint32, uint64> iter(_chords, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
      {
         if (iter.GetKey() < whichChord)
         {
            iter--; // move forward one, because we don't want to touch our left-of-whichChord chord, only those to the right of (whichChord)
            MoveChordsBackOneStartingAt(iter.GetKey());
            return;
         }
      }

      // If we got here, move everything back one
      MoveChordsBackOneStartingAt(*_chords.GetFirstKey());
   }
}

void MusicSheet :: MoveChordsBackOneStartingAt(uint32 whichChord)
{
   for (HashtableIterator<uint32, uint64> iter(_chords, whichChord, 0); iter.HasData(); iter++)
   {
      const uint32 chordIdx   = iter.GetKey();   // gotta copy these out since RemoveChord()
      const uint64 chordValue = iter.GetValue(); // will invalidate both of them
      (void) RemoveChord(chordIdx);              // guaranteed to succeed
      (void) PutChord(chordIdx-1, chordValue);   // guaranteed to succeed
   }
}

String MusicSheet :: ToString() const
{
   String ret;
   char buf[256]; muscleSprintf(buf, "MusicSheet %p has " UINT32_FORMAT_SPEC " chords, _usedNotes is " XINT64_FORMAT_SPEC ", _checksum is " UINT32_FORMAT_SPEC "\n", this, _chords.GetNumItems(), _usedNotes, _checksum);
   ret = buf;
   ret += String("_songFilePath is [%1]\n").Arg(_songFilePath);

   for (HashtableIterator<uint32, uint64> iter(_chords); iter.HasData(); iter++) 
   {
      muscleSprintf(buf, "   [" UINT32_FORMAT_SPEC "] -> " XINT64_FORMAT_SPEC "\n", iter.GetKey(), iter.GetValue());
      ret += buf;
   }
   return ret;
}

ConstMessageRef MusicSheet :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   switch(seniorDoMsg()->what)
   {
      case CHOIR_COMMAND_TOGGLE_NOTE:
      {
         const uint32 chordIdx = seniorDoMsg()->GetInt32(CHOIR_NAME_CHORD_INDEX);
         const uint32 noteIdx  = seniorDoMsg()->GetInt32(CHOIR_NAME_NOTE_INDEX);

         uint64 curChord = GetChordAtIndex(chordIdx, false);
         uint64 newChord = curChord ^ (1LL<<noteIdx);
         if (PutChord(chordIdx, newChord).IsOK())
         {
            MessageRef juniorMsg = GetMessageFromPool(CHOIR_COMMAND_SET_CHORD);
            if ((juniorMsg())&&(juniorMsg()->AddInt32(CHOIR_NAME_CHORD_INDEX, chordIdx).IsOK())&&(juniorMsg()->AddInt64(CHOIR_NAME_CHORD_VALUE, newChord).IsOK())) 
            {
               SendMessageToGUI(juniorMsg, true);
               return AddConstToRef(juniorMsg);
            }
         }
         else LogTime(MUSCLE_LOG_ERROR, "MusicSheet::SeniorUpdate():  Unable to toggle note " UINT32_FORMAT_SPEC " at chord index " UINT32_FORMAT_SPEC "\n", noteIdx, chordIdx);
      }
      break;

      case CHOIR_COMMAND_INSERT_CHORD: 
      {
         InsertChordAt(seniorDoMsg()->GetInt32(CHOIR_NAME_CHORD_INDEX));
         SendMessageToGUI(seniorDoMsg, false);
         return seniorDoMsg;
      }
      break;

      case CHOIR_COMMAND_DELETE_CHORD:
      {
         DeleteChordAt(seniorDoMsg()->GetInt32(CHOIR_NAME_CHORD_INDEX));
         SendMessageToGUI(seniorDoMsg, false);
         return seniorDoMsg;
      }
      break;

      case CHOIR_COMMAND_SET_SONG_FILE_PATH:
      {
         String newSongPath = seniorDoMsg()->GetString(CHOIR_NAME_SONG_FILE_PATH);
         SetSongFilePath(newSongPath);
         SendMessageToGUI(seniorDoMsg, false); // tell the GUI thread about the change also
         return seniorDoMsg;
      }
      break;

      case CHOIR_COMMAND_NOOP:
         return seniorDoMsg;

      default:
         LogTime(MUSCLE_LOG_ERROR, "MusicSheet::SeniorUpdate():  Unknown message code " UINT32_FORMAT_SPEC "\n", seniorDoMsg()->what);
      break;
   }

   LogTime(MUSCLE_LOG_ERROR, "MusicSheet::SeniorUpdate() failed!\n");
   return MessageRef();
}

status_t MusicSheet :: JuniorUpdate(const ConstMessageRef & juniorDoMsg)
{
   switch(juniorDoMsg()->what)
   {
      case CHOIR_COMMAND_NOOP:
         return B_NO_ERROR;

      case CHOIR_COMMAND_SET_CHORD:
      {
         const uint32 chordIdx = juniorDoMsg()->GetInt32(CHOIR_NAME_CHORD_INDEX);
         const uint64 chordVal = juniorDoMsg()->GetInt64(CHOIR_NAME_CHORD_VALUE);

         status_t ret;
         if (PutChord(chordIdx, chordVal).IsError(ret))
         {
            LogTime(MUSCLE_LOG_ERROR, "MusicSheet::JuniorUpdate():  Unable to set chord index " UINT32_FORMAT_SPEC " to chord value " XINT64_FORMAT_SPEC " [%s]\n", chordIdx, chordVal, ret());
            return ret;
         }

         SendMessageToGUI(juniorDoMsg, true); // tell the GUI thread about the change also
         return B_NO_ERROR;
      }

      case CHOIR_COMMAND_INSERT_CHORD: 
      {
         InsertChordAt(juniorDoMsg()->GetInt32(CHOIR_NAME_CHORD_INDEX));
         SendMessageToGUI(juniorDoMsg, false);
         return B_NO_ERROR;
      }

      case CHOIR_COMMAND_DELETE_CHORD:
      {
         DeleteChordAt(juniorDoMsg()->GetInt32(CHOIR_NAME_CHORD_INDEX));
         SendMessageToGUI(juniorDoMsg, false);
         return B_NO_ERROR;
      }

      case CHOIR_COMMAND_SET_SONG_FILE_PATH:
      {
         SetSongFilePath(juniorDoMsg()->GetString(CHOIR_NAME_SONG_FILE_PATH));
         SendMessageToGUI(juniorDoMsg, false); // tell the GUI thread about the change also
         return B_NO_ERROR;
      }

      case MUSIC_TYPE_MUSIC_SHEET:
         return SetFromArchive(juniorDoMsg);
      
      default:
         LogTime(MUSCLE_LOG_ERROR, "MusicSheet::JuniorUpdate():  Unknown message code " UINT32_FORMAT_SPEC "\n", juniorDoMsg()->what);
      break;
   }

   LogTime(MUSCLE_LOG_ERROR, "MusicSheet::JuniorUpdate() failed!\n");
   return B_BAD_ARGUMENT;
}

}; // end namespace choir

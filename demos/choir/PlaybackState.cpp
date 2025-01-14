#include "PlaybackState.h"
#include "MusicSheet.h"
#include "ChoirProtocol.h"

namespace choir {

PlaybackState :: PlaybackState()
{
   SetToDefaultStateAux();
}

PlaybackState :: PlaybackState(ZGDatabasePeerSession * session, int32 dbID) : MusicDatabaseObject(session, dbID)
{
   SetToDefaultStateAux();
}

PlaybackState :: ~PlaybackState()
{
   // empty
}

void PlaybackState :: SetToDefaultState()
{
   SetToDefaultStateAux();
   (void) SendFullStateToGUI(true);
}

void PlaybackState :: SetToDefaultStateAux()
{
   _networkStartTimeMicros = MUSCLE_TIME_NEVER;
   _microsPerChord         = DEFAULT_MICROSECONDS_PER_CHORD;
   _pausedIndex            = 0;
   _loop                   = false;
}

status_t PlaybackState :: SetFromArchive(const ConstMessageRef & archiveRef)
{
   const Message & archive = *archiveRef();
   if (archive.what != MUSIC_TYPE_PLAYBACK_STATE) return B_TYPE_MISMATCH;

   _networkStartTimeMicros = archive.GetInt64("start_time");
   _microsPerChord         = archive.GetInt64("us_per_chord");
   _pausedIndex            = archive.GetInt32("paused_idx");
   _loop                   = archive.GetBool("loop");

   SendMessageToGUI(archiveRef, true);
   return B_NO_ERROR;
}

status_t PlaybackState :: SaveToArchive(const MessageRef & archiveRef) const
{
   Message & archive = *archiveRef();
   archive.what = MUSIC_TYPE_PLAYBACK_STATE;
   return archive.AddInt64("start_time",   _networkStartTimeMicros) |
          archive.AddInt64("us_per_chord", _microsPerChord)         |
          archive.AddInt32("paused_idx",   _pausedIndex)            |
          archive.AddBool("loop",          _loop);
}

void PlaybackState :: SetMicrosPerChord(uint64 microsPerChord, uint64 optNetworkNow)
{
   if ((optNetworkNow != MUSCLE_TIME_NEVER)&&(IsPaused() == false))
   {
      const double curSeekPosInChords = (_microsPerChord>0) ? (((double)GetPlaybackPositionForNetworkTimeMicroseconds(optNetworkNow))/_microsPerChord) : 0;
      _networkStartTimeMicros = optNetworkNow - (curSeekPosInChords*microsPerChord);
   }
   _microsPerChord = microsPerChord;
}

uint32 PlaybackState :: CalculateChecksum() const
{
   return CalculatePODChecksum(_networkStartTimeMicros)
        + CalculatePODChecksum(_microsPerChord)
        + _pausedIndex
        + (_loop?1:0);
}

void PlaybackState :: StartPlayback(uint64 networkNow)
{
   if (IsPaused())
   {
      _networkStartTimeMicros = (networkNow-(_pausedIndex*_microsPerChord));
      _pausedIndex = MUSCLE_NO_LIMIT;
   }
}

void PlaybackState :: PausePlayback(uint64 networkNow)
{
   if (IsPaused() == false)
   {
      _pausedIndex = ((_microsPerChord>0)&&(_networkStartTimeMicros < networkNow)) ? ((networkNow-_networkStartTimeMicros)/_microsPerChord) : 0;
      _networkStartTimeMicros = MUSCLE_TIME_NEVER;
   }
}

void PlaybackState :: SeekTo(uint64 networkNow, uint32 whichChord)
{
   if (IsPaused()) _pausedIndex = whichChord;
              else _networkStartTimeMicros = (networkNow-(whichChord*_microsPerChord));
}

uint32 PlaybackState :: GetChordIndexForNetworkTimeStamp(uint64 networkTimeStamp, uint32 optLoopLengthChords) const
{
   if (IsPaused()) return _pausedIndex;

   const uint64 timeOffsetMicros = ((optLoopLengthChords!=0)?(networkTimeStamp%optLoopLengthChords):networkTimeStamp);
   return (_microsPerChord>0) ? ((timeOffsetMicros/_networkStartTimeMicros)/_microsPerChord) : 0;
}

uint64 PlaybackState :: GetNetworkTimeToPlayChord(uint32 chordIndex) const
{
   return IsPaused() ? MUSCLE_TIME_NEVER : (_networkStartTimeMicros+(_microsPerChord*chordIndex));
}

int64 PlaybackState :: GetPlaybackPositionForNetworkTimeMicroseconds(uint64 networkTimestamp) const
{
   return IsPaused() ? (_microsPerChord*_pausedIndex) : (networkTimestamp-_networkStartTimeMicros);
}

String PlaybackState :: ToString() const
{
   char buf[256];
   muscleSprintf(buf, "NetworkStartTimeMicros=" UINT64_FORMAT_SPEC " MicrosPerChord=" UINT64_FORMAT_SPEC " pausedIndex=" UINT32_FORMAT_SPEC " loop=%i\n", _networkStartTimeMicros, _microsPerChord, _pausedIndex, _loop);
   return buf;
}

ConstMessageRef PlaybackState :: SeniorUpdate(const ConstMessageRef & seniorDoMsg)
{
   switch(seniorDoMsg()->what)
   {
      case CHOIR_COMMAND_PLAY: case CHOIR_COMMAND_PAUSE: case CHOIR_COMMAND_ADJUST_PLAYBACK:
      {
         uint64 newMicrosPerChord;
         bool useMicrosPerChord = (seniorDoMsg()->FindInt64(CHOIR_NAME_MICROS_PER_CHORD, newMicrosPerChord).IsOK());

         uint32 newSeek;
         bool useSeek = (seniorDoMsg()->FindInt32(CHOIR_NAME_CHORD_INDEX, newSeek).IsOK());

         bool isLoop;
         bool useLoop = (seniorDoMsg()->FindBool(CHOIR_NAME_LOOP, isLoop).IsOK());

         return SeniorAdjustPlaybackState(seniorDoMsg()->what, useMicrosPerChord?&newMicrosPerChord:NULL, useSeek?&newSeek:NULL, useLoop?&isLoop:NULL);
      }
      break;

      case CHOIR_COMMAND_NOOP:
         return seniorDoMsg;

      default:
         LogTime(MUSCLE_LOG_ERROR, "PlaybackState::SeniorUpdate():  Unknown message code " UINT32_FORMAT_SPEC "\n", seniorDoMsg()->what);
      break;
   }

   LogTime(MUSCLE_LOG_ERROR, "PlaybackState::SeniorUpdate() failed!\n");
   return MessageRef();
}

status_t PlaybackState :: JuniorUpdate(const ConstMessageRef & juniorDoMsg)
{
   switch(juniorDoMsg()->what)
   {
      case CHOIR_COMMAND_NOOP:
         return B_NO_ERROR;

      case MUSIC_TYPE_PLAYBACK_STATE:
         return SetFromArchive(juniorDoMsg);

      default:
         LogTime(MUSCLE_LOG_ERROR, "PlaybackState::JuniorUpdate():  Unknown message code " UINT32_FORMAT_SPEC "\n", juniorDoMsg()->what);
      break;
   }

   LogTime(MUSCLE_LOG_ERROR, "PlaybackState::JuniorUpdate() failed!\n");
   return B_BAD_ARGUMENT;
}

MessageRef PlaybackState :: SeniorAdjustPlaybackState(uint32 whatCode, const uint64 * optNewMicrosPerChord, const uint32 * optSeekTo, bool * optSetLoop)
{
   const uint64 networkNow = GetNetworkTime64();

   if (optSetLoop)
   {
      const MusicSheet * musicSheet = static_cast<const MusicSheet *>(GetDatabaseObject(CHOIR_DATABASE_SCORE));
      if (musicSheet == NULL)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "PlaybackState::SeniorAdjustPlaybackState():  MusicSheet not available, perhaps we're not getting called from the right context?\n");
         return MessageRef();
      }

      if ((IsPaused() == false)&&(IsLoop())&&(*optSetLoop == false)&&(musicSheet->GetSongLengthInChords(false) > 0))
      {
         // Seek the deck to its per-loop offset so that playback can continue from where it's at in the current loop
         // rather than jumping way off the right-hand side as it would do otherwise
         const uint64 songDurationMicros = GetMicrosPerChord()*musicSheet->GetSongLengthInChords(false);
         if (songDurationMicros > 0) SetNetworkStartTimeMicros(networkNow-((networkNow-GetNetworkStartTimeMicros()) % songDurationMicros));
      }
      SetLoop(*optSetLoop);
   }

   switch(whatCode)
   {
      case CHOIR_COMMAND_PLAY:
         if (IsPaused())
         {
            if (optNewMicrosPerChord) SetMicrosPerChord(*optNewMicrosPerChord);
            if (optSeekTo) SeekTo(networkNow, *optSeekTo);
            StartPlayback(networkNow);
         }
      break;

      case CHOIR_COMMAND_PAUSE:
         if (!IsPaused()) PausePlayback(networkNow);
         if (optNewMicrosPerChord) SetMicrosPerChord(*optNewMicrosPerChord);
         if (optSeekTo) SeekTo(networkNow, *optSeekTo);
      break;

      case CHOIR_COMMAND_ADJUST_PLAYBACK:
         // to do:  make this more clever
         if (optNewMicrosPerChord) SetMicrosPerChord(*optNewMicrosPerChord, optSeekTo?MUSCLE_TIME_NEVER:networkNow);
         if (optSeekTo) SeekTo(networkNow, *optSeekTo);
      break;
   }

   MessageRef juniorMsg = GetMessageFromPool();
   if ((juniorMsg())&&(SaveToArchive(juniorMsg).IsError())) juniorMsg.Reset();
   if (juniorMsg()) SendMessageToGUI(juniorMsg, false); // tell the GUI thread about the change also

   return juniorMsg;
}

}; // end namespace choir

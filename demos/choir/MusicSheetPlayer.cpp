#include <QTimer>
#include "syslog/SysLog.h"  // for GetHumanReadableSignedTimeIntervalString()
#include "MusicSheetPlayer.h"

namespace choir {

MusicSheetPlayer :: MusicSheetPlayer(const INetworkTimeProvider * networkTimeProvider, QObject * parent)
   : QObject(parent)
   , _networkTimeProvider(networkTimeProvider)
   , _nextChordIndex(0)
   , _nextWakeupTimeLocal(MUSCLE_TIME_NEVER)
   , _wakeupTimer(NULL)
{
   // empty
}

MusicSheetPlayer :: ~MusicSheetPlayer()
{
   // empty
}

void MusicSheetPlayer :: SetupTimer()
{
   _wakeupTimer = new QTimer(this);
   _wakeupTimer->setTimerType(Qt::PreciseTimer);
   _wakeupTimer->setSingleShot(true);
   connect(_wakeupTimer, SIGNAL(timeout()), this, SLOT(Wakeup()));
   RecalculateWakeupTime();  // just in case
}

void MusicSheetPlayer :: DestroyTimer()
{
   delete _wakeupTimer;
   _wakeupTimer = NULL;
}

void MusicSheetPlayer :: MusicSheetUpdated(const ConstMusicSheetRef & newMusicSheet)
{
   _musicSheet = newMusicSheet;
   RecalculateWakeupTime();
}

void MusicSheetPlayer :: PlaybackStateUpdated(const PlaybackState & newPlaybackState)
{
   _playbackState = newPlaybackState;
   _nextChordIndex = _playbackState.GetChordIndexForNetworkTimeStamp(_networkTimeProvider->GetNetworkTime64(), _musicSheet()?_musicSheet()->GetSongLengthInChords(_playbackState.IsLoop()):0);
   RecalculateWakeupTime();
}

void MusicSheetPlayer :: RecalculateWakeupTime()
{
   uint64 nextWakeupTimeLocal = MUSCLE_TIME_NEVER;

   const uint32 songDurationChords = _musicSheet() ? _musicSheet()->GetSongLengthInChords(_playbackState.IsLoop()) : 0;
   if (songDurationChords > 0)
   {
      if ((_playbackState.IsPaused() == false)&&(_nextChordIndex < songDurationChords))
      {
         // Figure out when the next note in the music sheet is supposed to play
         while(_nextChordIndex < songDurationChords)
         {
            if (_musicSheet()->GetChordAtIndex(_nextChordIndex, _playbackState.IsLoop()) != 0) break;
                                                                                          else _nextChordIndex++;
         }

         nextWakeupTimeLocal = _networkTimeProvider->GetRunTime64ForNetworkTime64(_playbackState.GetNetworkTimeToPlayChord(_nextChordIndex));
      }
   }

   if (_wakeupTimer)
   {
      _nextWakeupTimeLocal = nextWakeupTimeLocal;

      if (_nextWakeupTimeLocal == MUSCLE_TIME_NEVER) _wakeupTimer->stop();
      else
      {
         // Set the timer to wake us up at our specified time
         int64 microsUntil = _nextWakeupTimeLocal-GetRunTime64();
         if (microsUntil < 0) microsUntil = 0;
         _wakeupTimer->start(MicrosToMillis(microsUntil)+((microsUntil%1000)?1:0));   // round up to the nearest millisecond
      }
   }
}

void MusicSheetPlayer :: Wakeup()
{
   const uint32 songDurationChords = _musicSheet() ? _musicSheet()->GetSongLengthInChords(_playbackState.IsLoop()) : 0;
   if ((songDurationChords > 0)&&(_nextWakeupTimeLocal != MUSCLE_TIME_NEVER))
   {
      const uint64 localNow = GetRunTime64();
      if ((_playbackState.IsPaused() == false)&&(_nextChordIndex < songDurationChords))
      {
         uint64 networkNow = _networkTimeProvider->GetNetworkTime64ForRunTime64(localNow);

         // Find the chord index that is most recently in our past, and play that
         uint64 chordToPlay   = 0;
         uint64 chordPlayTime = 0;
         while(_nextChordIndex < _musicSheet()->GetSongLengthInChords(_playbackState.IsLoop()))
         {
            const uint64 chordNetworkTime = _playbackState.GetNetworkTimeToPlayChord(_nextChordIndex);
            if (networkNow >= chordNetworkTime)
            {
               const uint64 chord = _musicSheet()->GetChordAtIndex(_nextChordIndex, _playbackState.IsLoop());
               if (chord != 0)
               {
                  chordToPlay   = chord;
                  chordPlayTime = chordNetworkTime;
               }
               _nextChordIndex++;
            }
            else break;
         }

         if (chordToPlay)
         {
            const uint64 lateBy = (networkNow-chordPlayTime);
            if (lateBy < (uint64)MillisToMicros(50))  // if we're more than 50mS late, then we won't bother
            {
               emit RequestBells(chordToPlay, true);
            }
            //else LogTime(MUSCLE_LOG_INFO, "Skipping chord #" UINT32_FORMAT_SPEC " because I'm late by [%s] to play it\n", _nextChordIndex, GetHumanReadableUnsignedTimeIntervalString(lateBy)());
         }
      }
   }

   RecalculateWakeupTime();
}

}; // end namespace choir

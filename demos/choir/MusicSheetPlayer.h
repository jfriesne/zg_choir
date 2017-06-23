#ifndef MusicSheetPlayer_h
#define MusicSheetPlayer_h

#include <QObject>

class QTimer;

#include "zg/INetworkTimeProvider.h"
#include "MusicSheet.h"
#include "PlaybackState.h"

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
Q_DECLARE_METATYPE(choir::ConstMusicSheetRef);
Q_DECLARE_METATYPE(choir::PlaybackState);
#endif

namespace choir {

/** This object is in charge of reading the music sheet and telling Quasimodo
  * when to ring the bells, based on the current time and settings.
  * This is done within a separate thread, so that the timing of the bell-ringing won't be affected GUI operations
  */
class MusicSheetPlayer : public QObject
{
Q_OBJECT

public:
   /** Constructor */
   MusicSheetPlayer(const INetworkTimeProvider * networkTimeProvider, QObject * parent = NULL);

   /** Destructor */
   ~MusicSheetPlayer();
   
signals:
   /** Emitted when we want Quasimodo to ring a specified set of bells */
   void RequestBells(quint64 chord, bool localNotesOnly);

public slots:
   /** Received on startup */
   void SetupTimer();

   /** Received when the sheet music has changed */
   void MusicSheetUpdated(const choir::ConstMusicSheetRef & newMusicSheet);

   /** Received when the playback state has changed */
   void PlaybackStateUpdated(const choir::PlaybackState & newPlaybackState);

   /** Received on shutdown */
   void DestroyTimer();

private slots:
   void Wakeup();

private:
   void RecalculateWakeupTime();

   const INetworkTimeProvider * _networkTimeProvider;

   ConstMusicSheetRef _musicSheet;
   PlaybackState _playbackState;

   uint32 _nextChordIndex;  // index of the next chord-position in the music sheet that we haven't played yet

   uint64 _nextWakeupTimeLocal;
   QTimer * _wakeupTimer;
};

}; // end namespace choir

#endif

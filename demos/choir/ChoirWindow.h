#ifndef ChoirWindow_h
#define ChoirWindow_h

#include <QMainWindow>
#include <QThread>

#include "qtsupport/QMessageTransceiverThread.h"

#include "ChoirNameSpace.h"
#include "ChoirProtocol.h"
#include "ChoirThread.h"

#include "MusicData.h"
#include "MusicSheetPlayer.h"
#include "NoteAssignmentsMap.h"
#include "Quasimodo.h"

class QBoxLayout;
class QComboBox;
class QLabel;
class QScrollBar;
class QSlider;
class QToolButton;

namespace choir {

class MusicSheetWidget;
class RosterWidget;

/** This is the main window of the ZGChoir application.  All top-level GUI functionality is handled here. */
class ChoirWindow : public QMainWindow
{
Q_OBJECT

public:
   /** Default constructor */
   ChoirWindow();

   /** Destructor */
   virtual ~ChoirWindow();

   /** Called when the ZGChoir window is resized */
   virtual void resizeEvent(QResizeEvent *);

   /** Called when the user presses a key while the ZGChoir has the focus */
   virtual void keyPressEvent(QKeyEvent *);

signals:
   /** This signal is emitted at startup to tell the Quasimodo thread to do his setup (start timers, load samples, etc) */
   void SetupTheBells();

   /** This signal is emitted at startup to tell the Quasimodo thread to ring some bells now
     * @param bellChord a bit-chord of CHOIR_NOTE_* values to play
     * @param localNotesOnly if true, Quasidomodo will ring only those bells in (bellChord) that are assigned to this peer.
     *                       otherwise Quasimodo will ring all the bells in (bellChord)
     */
   void RequestBells(quint64 bellChord, bool localNotesOnly);

   /** This signal is emitted when the set of bells assigned to us changed, to let Quasimodo know which bells to use now.
     * @param localNotesChord a bit-chord of CHOIR_NOTE_* values indicating which bells are currently assigned to this peer.
     */ 
   void LocalNoteAssignmentsChanged(quint64 localNotesChord);

   /** This signal is emitted at shutdown to tell the Quasimodo thread to free his resources in preparation for exit */
   void DestroyTheBells();

   /** This signal is emitted at startup to tell the music-sheet-player thread to do his setup */
   void SetupTimer();

   /** This signal is emitted when our MusicSheet changes, in order to send the new MusicSheet to the music-sheet-player thread
     * @param newMusicSheet a read-only reference to the new MusicSheet object 
     */
   void SendMusicSheetToPlayer(const choir::ConstMusicSheetRef & newMusicSheet);

   /** This signal is emitted when our PlaybackState changes, in order to send the new PlaybackState to the music-sheet-player thread
     * @param newPlaybackState the new playback state for the MusicPlayer to use to control its playback behavior
     */
   void SendPlaybackStateToPlayer(const choir::PlaybackState & newPlaybackState);

   /** This signal is emitted at shutdown to tell the music-player thread to free his resources in preparation for exit */
   void DestroyTimer();

private slots:
   void MessageReceivedFromServer(const MessageRef & msg, const String & sessionID);
   void UpdateButtons();
   void NotePositionClicked(uint32 chordIdx, int noteIdx);
   void SeekRequested(uint32 chordIdx);
   void PlayButtonClicked()  {SendTransportCommand(CHOIR_COMMAND_PLAY,  NULL);}
   void PauseButtonClicked() {SendTransportCommand(CHOIR_COMMAND_PAUSE, NULL);}
   void StopButtonClicked()  {const uint32 topIdx = 0; SendTransportCommand(CHOIR_COMMAND_PAUSE, &topIdx);}
   void LoopButtonClicked();
   void LoopMenuItemSelected();
   void SendUpdatedMusicSheetToPlayer();
   void SendUpdatedPlaybackStateToPlayer();
   void ClearSong();
   void OpenSong();
   void SaveSong();
   void CloneWindow();
   void UpdateHorizontalScrollBarSettings();
   void UpdateVerticalScrollBarSettings();
   void UserBeganDraggingTempoSlider();
   void TempoChangeRequested(int sliderVal);
   void UserStoppedDraggingTempoSlider();
   void UpdateTempoValueLabel();
   void UpdateTempoSliderFromPlaybackState();
   void RosterBellPositionClicked(const zg::ZGPeerID & peerID, uint32 noteIdx);
   void RosterWheelTurned(int deltaY);
   void UserRequestedStrategyChange(int newStrategy);
   void AutoScrollMusicSheetWidget(int pixelsToTheRight);
   void DeleteChord();
   void InsertChord();
   void SetAutomaticBellAssignment() {UserRequestedStrategyChange(ASSIGNMENT_STRATEGY_AUTOMATIC);}
   void SetAssistedBellAssignment()  {UserRequestedStrategyChange(ASSIGNMENT_STRATEGY_ASSISTED);}
   void SetManualBellAssignment()    {UserRequestedStrategyChange(ASSIGNMENT_STRATEGY_MANUAL);}

private:
   QToolButton * CreateButton(const QString & resourcePath, const char * slotName, bool isToggleButton, QBoxLayout * layout);
   status_t UploadMusicSheet(const MusicSheet & musicSheet);
   void SendTransportCommand(uint32 what, const uint32 * optSeekToChordIdx);
   void MusicSheetUpdated();
   void NoteAssignmentsUpdated();
   QString GetLocalPeerNickname() const;
   const ZGPeerID & GetLocalPeerID() const;
   QString GetStrategyName(uint32 whichStrategy) const;
   QAction * CreateMenuItem(QMenu * menu, const QString & label, QObject * target, const char * slotName, const QString & optShortcut = QString(), bool isCheckable = false);

   void ScheduleSendUpdatedMusicSheetToPlayer();
   void ScheduleSendUpdatedPlaybackStateToPlayer();

   ChoirThread _serverThread;

   MusicSheet _musicSheet;
   MusicSheetWidget * _musicSheetWidget;
   QScrollBar * _horizontalScrollBar;

   NoteAssignmentsMap _assigns;
   RosterWidget * _rosterWidget;
   QScrollBar * _verticalScrollBar;
   QWidget * _padRightWidget;

   QToolButton * _clearButton;

   QToolButton * _openButton;
   QToolButton * _saveButton;

   QToolButton * _deleteButton;
   QToolButton * _insertButton;

   QToolButton * _playButton;
   QToolButton * _pauseButton;
   QToolButton * _stopButton;

   QToolButton * _loopButton;

   QAction * _openMenuItem;
   QAction * _saveMenuItem;
   QAction * _clearMenuItem;
   QAction * _deleteMenuItem;
   QAction * _insertMenuItem;
   QAction * _playMenuItem;
   QAction * _pauseMenuItem;
   QAction * _stopMenuItem;
   QAction * _loopMenuItem;
   QAction * _automaticMenuItem;
   QAction * _assistedMenuItem;
   QAction * _manualMenuItem;
   QAction * _cloneMenuItem;

   QComboBox * _strategyComboBox;

   QSlider * _tempoSlider;
   bool _userIsDraggingTempoSlider;
   QLabel * _tempoValueLabel;

   QToolButton * _cloneWindowButton;

   PlaybackState _playbackState;

   QThread _quasimodoThread;
   Quasimodo _quasimodo;

   QThread _musicPlayerThread;
   MusicSheetPlayer _musicPlayer;

   bool _sendPlaybackStateToPlayerPending;
   bool _sendMusicSheetToPlayerPending;
};

}; // end namespace choir

#endif

#include <QApplication>
#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QToolButton>

#include "system/SystemInfo.h"

#include "ChoirProtocol.h"
#include "ChoirWindow.h"
#include "MusicSheetWidget.h"
#include "RosterWidget.h"

#include "zg/ZGConstants.h"  // for PeerInfoToString()

namespace choir {

static ZGPeerSettings GetChoirPeerSettings()
{
   ZGPeerSettings settings("ZGChoir", "ZGChoir", NUM_CHOIR_DATABASES, false);
   settings.SetApplicationPeerCompatibilityVersion(CHOIR_APP_COMPATIBILITY_VERSION);

   MessageRef msg = GetMessageFromPool();
   if (msg())
   {
      // Derive a more-or-less human-readable name-string for this computer, from the hostname
      String lhn = GetLocalHostName();
      lhn = lhn.Substring(0, ".");
      lhn = lhn.Substring(0, "-");
      lhn.Replace(" ", "");
      if (lhn.HasChars()) lhn = lhn.Substring(0,1).ToUpperCase()+lhn.Substring(1,9).ToLowerCase();
      (void) msg()->AddString(CHOIR_NAME_PEER_NICKNAME, lhn);
   }
   settings.SetPeerAttributes(msg);

   return settings;
}

ChoirWindow :: ChoirWindow() 
   : _serverThread(GetChoirPeerSettings())
   , _userIsDraggingTempoSlider(false)
   , _musicPlayer(_serverThread.GetNetworkTimeProvider())
   , _sendPlaybackStateToPlayerPending(false)
   , _sendMusicSheetToPlayerPending(false)
{
   qRegisterMetaType<choir::ConstMusicSheetRef>();
   qRegisterMetaType<choir::PlaybackState>();

   setWindowTitle(tr("ZG Choir Demo v%1 - ").arg(CHOIR_VERSION_STRING) + GetLocalPeerNickname());
   setAttribute(Qt::WA_DeleteOnClose);
   resize(800, 600);

   QWidget * central = new QWidget;
   setCentralWidget(central);

   QBoxLayout * vbl = new QBoxLayout(QBoxLayout::TopToBottom, central);
   vbl->setSpacing(3);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
   vbl->setMargin(2);
#else
   vbl->setContentsMargins(2,2,2,2);
#endif

   QWidget * musicAndScroll = new QWidget;
   {
      QBoxLayout * sbl = new QBoxLayout(QBoxLayout::TopToBottom, musicAndScroll);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
      sbl->setMargin(0);
#else
      sbl->setContentsMargins(0,0,0,0);
#endif
      sbl->setSpacing(0);

      _musicSheetWidget = new MusicSheetWidget(_serverThread.GetNetworkTimeProvider());
      _musicSheetWidget->setFixedHeight(150);
      connect(_musicSheetWidget, SIGNAL(NotePositionClicked(uint32, int)), this, SLOT(NotePositionClicked(uint32, int)));
      connect(_musicSheetWidget, SIGNAL(SeekRequested(uint32)), this, SLOT(SeekRequested(uint32)));
      connect(_musicSheetWidget, SIGNAL(AutoScrollMusicSheetWidget(int)), this, SLOT(AutoScrollMusicSheetWidget(int)));
      sbl->addWidget(_musicSheetWidget);

      _horizontalScrollBar = new QScrollBar(Qt::Horizontal);
      connect(_horizontalScrollBar, SIGNAL(valueChanged(int)), _musicSheetWidget, SLOT(SetHorizontalScrollOffset(int)));
      connect(_musicSheetWidget, SIGNAL(SongWidthChanged()), this, SLOT(UpdateHorizontalScrollBarSettings()));
     
      sbl->addWidget(_horizontalScrollBar);
   }
   vbl->addWidget(musicAndScroll);

   QWidget * buttonsRow = new QWidget;
   {
      QBoxLayout * hbl = new QBoxLayout(QBoxLayout::LeftToRight, buttonsRow);
      hbl->setSpacing(3);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
      hbl->setMargin(3);
#else
      hbl->setContentsMargins(3,3,3,3);
#endif

      _clearButton = CreateButton(":/choir_clear_song.png", SLOT(ClearSong()), false, hbl);
      hbl->addSpacing(10);
      _openButton  = CreateButton(":/choir_open_file.png",  SLOT(OpenSong()),  false, hbl);
      _saveButton  = CreateButton(":/choir_save_file.png",  SLOT(SaveSong()),  false, hbl);

      hbl->addStretch();

      _deleteButton = CreateButton(":/choir_delete.png",  SLOT(DeleteChord()),  false, hbl);
      _insertButton = CreateButton(":/choir_insert.png",  SLOT(InsertChord()),  false, hbl);

      hbl->addStretch();

      _playButton  = CreateButton(":/choir_play.png",  SLOT(PlayButtonClicked()),  true, hbl);
      _pauseButton = CreateButton(":/choir_pause.png", SLOT(PauseButtonClicked()), true, hbl);
      _stopButton  = CreateButton(":/choir_stop.png",  SLOT(StopButtonClicked()),  true, hbl);
      hbl->addStretch();

      _loopButton  = CreateButton(":/choir_loop.png",  SLOT(LoopButtonClicked()),  true, hbl);
      hbl->addStretch();

      QLabel * assLabel = new QLabel("Bell Assignment: ");
      hbl->addWidget(assLabel);
      
      _strategyComboBox = new QComboBox;
      for (uint32 i=0; i<NUM_ASSIGNMENT_STRATEGIES; i++) _strategyComboBox->addItem(GetStrategyName(i));
      connect(_strategyComboBox, SIGNAL(activated(int)), this, SLOT(UserRequestedStrategyChange(int)));
      hbl->addWidget(_strategyComboBox);

      hbl->addStretch();

      QLabel * tempoLabel = new QLabel(tr("Tempo:"));
      hbl->addWidget(tempoLabel);

      _tempoSlider = new QSlider(Qt::Horizontal);
      _tempoSlider->setSingleStep(10);
      _tempoSlider->setMinimum(10);
      _tempoSlider->setMaximum(500);
      _tempoSlider->setValue(100);
      connect(_tempoSlider, SIGNAL(sliderPressed()),   this, SLOT(UserBeganDraggingTempoSlider()));
      connect(_tempoSlider, SIGNAL(sliderReleased()),  this, SLOT(UserStoppedDraggingTempoSlider()));
      connect(_tempoSlider, SIGNAL(sliderMoved(int)),  this, SLOT(TempoChangeRequested(int)));
      connect(_tempoSlider, SIGNAL(valueChanged(int)), this, SLOT(UpdateTempoValueLabel()));
      _tempoSlider->setFixedWidth(100);
      hbl->addWidget(_tempoSlider);

      _tempoValueLabel = new QLabel;
      const QString p100 = "100%";
#if QT_VERSION >= 0x050B00
      const int tw = _tempoValueLabel->fontMetrics().horizontalAdvance(p100);
#else
      const int tw = _tempoValueLabel->fontMetrics().width(p100);
#endif
      _tempoValueLabel->setFixedWidth(tw);
      hbl->addWidget(_tempoValueLabel);
      UpdateTempoValueLabel();

      hbl->addStretch();
   
      _cloneWindowButton = CreateButton(":/clone_choir_window.png", SLOT(CloneWindow()), true, hbl);
   }
   vbl->addWidget(buttonsRow);

   QWidget * rosterAndScroll = new QWidget;
   {
      QBoxLayout * hbl = new QBoxLayout(QBoxLayout::LeftToRight, rosterAndScroll);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
      hbl->setMargin(0);
#else
      hbl->setContentsMargins(0,0,0,0);
#endif
      hbl->setSpacing(0);
      {
         _rosterWidget = new RosterWidget(GetLocalPeerID());
         connect(_rosterWidget, SIGNAL(BellPositionClicked(const zg::ZGPeerID &, uint32)), this, SLOT(RosterBellPositionClicked(const zg::ZGPeerID &, uint32)));
         connect(_rosterWidget, SIGNAL(TotalContentHeightChanged()), this, SLOT(UpdateVerticalScrollBarSettings()));
         connect(_rosterWidget, SIGNAL(WheelTurned(int)), this, SLOT(RosterWheelTurned(int)));
      }
      hbl->addWidget(_rosterWidget, 1);

      _verticalScrollBar = new QScrollBar(Qt::Vertical);
      connect(_verticalScrollBar, SIGNAL(valueChanged(int)), _rosterWidget, SLOT(SetVerticalScrollOffset(int)));
      hbl->addWidget(_verticalScrollBar);

      _padRightWidget = new QWidget;
      _padRightWidget->setFixedWidth(5);
      hbl->addWidget(_padRightWidget);
   }
   vbl->addWidget(rosterAndScroll, 1);

    // Set up the menus
   QMenuBar * bar = menuBar();

   const QString ellipses = QChar(0x26, 0x20);

   QMenu * fileMenu = bar->addMenu(tr("&File"));
   _openMenuItem  = CreateMenuItem(fileMenu, tr("Open Song")+ellipses,  _openButton, SIGNAL(clicked()), "Ctrl+O");
   _saveMenuItem  = CreateMenuItem(fileMenu, tr("Save Song")+ellipses,  _saveButton, SIGNAL(clicked()), "Ctrl+S");
   fileMenu->addSeparator();
   _clearMenuItem = CreateMenuItem(fileMenu, tr("Clear Song")+ellipses, _clearButton, SIGNAL(clicked()));

   QMenu * editMenu = bar->addMenu(tr("&Edit"));
   _deleteMenuItem = CreateMenuItem(editMenu, tr("Remove Column At Seek Position"), _deleteButton, SIGNAL(clicked()), "Ctrl+K");
   _insertMenuItem = CreateMenuItem(editMenu, tr("Insert Column At Seek Position"), _insertButton, SIGNAL(clicked()), "Ctrl+I");

   QMenu * playbackMenu = bar->addMenu(tr("&Playback"));
   _playMenuItem  = CreateMenuItem(playbackMenu, tr("Play"),  _playButton,  SIGNAL(clicked()), "Ctrl+P");
   _pauseMenuItem = CreateMenuItem(playbackMenu, tr("Pause"), _pauseButton, SIGNAL(clicked()), "Ctrl+E");
   _stopMenuItem  = CreateMenuItem(playbackMenu, tr("Stop"),  _stopButton,  SIGNAL(clicked()), "Ctrl+T");
   playbackMenu->addSeparator();
   _loopMenuItem  = CreateMenuItem(playbackMenu, tr("Enable Looping"), this, SLOT(LoopMenuItemSelected()), "Ctrl+L", true);

   QMenu * assignMenu = bar->addMenu(tr("&Bell Assignment"));
   _automaticMenuItem = CreateMenuItem(assignMenu, tr("Automatic"), this, SLOT(SetAutomaticBellAssignment()), "Ctrl+1");
   _assistedMenuItem  = CreateMenuItem(assignMenu, tr("Assisted"),  this, SLOT(SetAssistedBellAssignment()),  "Ctrl+2");
   _manualMenuItem    = CreateMenuItem(assignMenu, tr("Manual"),    this, SLOT(SetManualBellAssignment()),    "Ctrl+3");

   QMenu * windowMenu = bar->addMenu(tr("&Window"));
   _cloneMenuItem = CreateMenuItem(windowMenu, tr("New Window"), _cloneWindowButton, SIGNAL(clicked()), "Ctrl+N");
   windowMenu->addSeparator();
   _cloneMenuItem = CreateMenuItem(windowMenu, tr("Close Window"), this, SLOT(close()), "Ctrl+W");

   connect(&_serverThread, SIGNAL(MessageReceived(const MessageRef &, const String &)), this, SLOT(MessageReceivedFromServer(const MessageRef &, const String &)));
   UpdateButtons();

   // start the ZG thread running
   if (_serverThread.StartInternalThread().IsError()) printf("Error, couldn't start network thread!\n");

   _quasimodo.moveToThread(&_quasimodoThread);  // do this first, so these connections will be Queued connections
   connect(this, SIGNAL(SetupTheBells()),                      &_quasimodo, SLOT(SetupTheBells()));
   connect(this, SIGNAL(LocalNoteAssignmentsChanged(quint64)), &_quasimodo, SLOT(SetLocalNoteAssignments(quint64)));
   connect(this, SIGNAL(RequestBells(quint64, bool)),          &_quasimodo, SLOT(RingSomeBells(quint64, bool)));
   connect(this, SIGNAL(DestroyTheBells()),                    &_quasimodo, SLOT(DestroyTheBells()), Qt::BlockingQueuedConnection);
   connect(&_quasimodo, SIGNAL(RangLocalBells(quint64)),     _rosterWidget, SLOT(AnimateLocalBells(quint64)));
   _quasimodoThread.start(QThread::HighPriority);  // we want him to render his audio buffers on time if possible, to avoid underruns
   emit SetupTheBells();  // tell quasimodo to get his bells together

   _musicPlayer.moveToThread(&_musicPlayerThread);  // do this first, so these connections will be Queued connections
   connect(this, SIGNAL(SetupTimer()),          &_musicPlayer, SLOT(SetupTimer()));
   connect(this, SIGNAL(SendMusicSheetToPlayer(const choir::ConstMusicSheetRef &)), &_musicPlayer, SLOT(MusicSheetUpdated(const choir::ConstMusicSheetRef &)));
   connect(this, SIGNAL(SendPlaybackStateToPlayer(const choir::PlaybackState &)),   &_musicPlayer, SLOT(PlaybackStateUpdated(const choir::PlaybackState &)));
   connect(this, SIGNAL(DestroyTimer()),        &_musicPlayer, SLOT(DestroyTimer()), Qt::BlockingQueuedConnection);
   connect(&_musicPlayer, SIGNAL(RequestBells(quint64, bool)), &_quasimodo, SLOT(RingSomeBells(quint64, bool)));
   _musicPlayerThread.start();
   emit SetupTimer();  // tell music player to set up his wakeup-timer
}

ChoirWindow :: ~ChoirWindow()
{
   _serverThread.ShutdownInternalThread();

   emit DestroyTimer();
   _musicPlayerThread.quit();
   _musicPlayerThread.wait();

   emit DestroyTheBells();
   _quasimodoThread.quit();
   _quasimodoThread.wait();
}

QAction * ChoirWindow :: CreateMenuItem(QMenu * menu, const QString & label, QObject * target, const char * slotName, const QString & optShortcut, bool isCheckable)
{
   QAction * a = menu->addAction(label, target, slotName);
   if (optShortcut.length() > 0) a->setShortcut(optShortcut);
   if (isCheckable) a->setCheckable(true);
   return a;
}

QString ChoirWindow :: GetStrategyName(uint32 whichStrategy) const
{
   switch(whichStrategy)
   {
      case ASSIGNMENT_STRATEGY_AUTOMATIC: return tr("Automatic");
      case ASSIGNMENT_STRATEGY_ASSISTED:  return tr("Assisted");
      case ASSIGNMENT_STRATEGY_MANUAL:    return tr("Manual");
      default:                            return tr("Unknown");
   }
}

QString ChoirWindow :: GetLocalPeerNickname() const
{
   return GetPeerNickname(GetLocalPeerID(), _serverThread.GetLocalPeerSettings().GetPeerAttributes())();
}

const ZGPeerID & ChoirWindow :: GetLocalPeerID() const
{
   return _serverThread.GetLocalPeerID();
}

static int MicrosecondsPerChordToTempo(const QSlider * slider, uint64 micros) {return (micros==0) ? slider->maximum() : (int)muscleClamp((uint64)((DEFAULT_MICROSECONDS_PER_CHORD*100)/micros), (uint64)slider->minimum(), (uint64)slider->maximum());}
static uint64 TempoToMicrosecondsPerChord(int tempo) {return (tempo>0) ? ((DEFAULT_MICROSECONDS_PER_CHORD*100)/tempo) : MUSCLE_TIME_NEVER;}

void ChoirWindow :: UserRequestedStrategyChange(int newStrategy)
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_SET_STRATEGY);
   if ((cmdMsg())&&(cmdMsg()->AddInt32(CHOIR_NAME_STRATEGY, newStrategy).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: TempoChangeRequested(int sliderVal)
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_ADJUST_PLAYBACK);
   if ((cmdMsg())&&(cmdMsg()->AddInt64(CHOIR_NAME_MICROS_PER_CHORD, TempoToMicrosecondsPerChord(sliderVal)).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: UpdateTempoValueLabel()
{
   _tempoValueLabel->setText(QString("%1%").arg(_tempoSlider->value()));
}

void ChoirWindow :: UpdateHorizontalScrollBarSettings()
{
   const int contentWidth = _musicSheetWidget->GetSongWidthPixels();
   const int newMaximum = muscleMax(0, contentWidth-_musicSheetWidget->width());
   _horizontalScrollBar->setHidden((contentWidth <= newMaximum)&&(_musicSheetWidget->GetHorizontalScrollOffset() == 0));
   _horizontalScrollBar->setMaximum(newMaximum);
}

void ChoirWindow :: UpdateVerticalScrollBarSettings()
{
   const int contentHeight = _rosterWidget->GetTotalContentHeight();
   _verticalScrollBar->setHidden(contentHeight <= _rosterWidget->height());
   _verticalScrollBar->setMaximum(muscleMax(1,contentHeight-_rosterWidget->height()));
   if (_verticalScrollBar->isHidden()) _rosterWidget->SetVerticalScrollOffset(0);
   _padRightWidget->setHidden(!_verticalScrollBar->isHidden());
}

void ChoirWindow :: RosterWheelTurned(int delta)
{
   if (_verticalScrollBar->isVisible()) _verticalScrollBar->setValue(muscleClamp(_verticalScrollBar->value()-(delta/30), _verticalScrollBar->minimum(), _verticalScrollBar->maximum()));
}

void ChoirWindow :: resizeEvent(QResizeEvent * e)
{
   QMainWindow::resizeEvent(e);
   UpdateHorizontalScrollBarSettings();  // in case we are now wide enough to hide the horizontal scroll bar, or not
   UpdateVerticalScrollBarSettings();    // in case we are now tall enough to hide the vertical scroll bar, or not
}

QToolButton * ChoirWindow :: CreateButton(const QString & resourcePath, const char * slotName, bool isToggleButton, QBoxLayout * layout)
{
   QToolButton * b = new QToolButton;
   b->setCheckable(isToggleButton);
   b->setIcon(QIcon(resourcePath));
   connect(b, SIGNAL(clicked()), this, slotName);
   layout->addWidget(b);
   return b;
}

void ChoirWindow :: NotePositionClicked(uint32 chordIdx, int noteIdx)
{
   const uint64 newChord = _musicSheet.GetChordAtIndex(chordIdx, false) ^ (1LL<<noteIdx);
   if (newChord) emit RequestBells(newChord, false);  // so the user can hear the chord he's assembling

   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_TOGGLE_NOTE);
   if ((cmdMsg())&&(cmdMsg()->AddInt32(CHOIR_NAME_CHORD_INDEX, chordIdx).IsOK())&&(cmdMsg()->AddInt32(CHOIR_NAME_NOTE_INDEX, noteIdx).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: RosterBellPositionClicked(const ZGPeerID & pid, uint32 noteIdx)
{
   if (_assigns.GetAssignmentStrategy() != ASSIGNMENT_STRATEGY_AUTOMATIC)
   {
      MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_TOGGLE_ASSIGNMENT);
      if ((cmdMsg())&&(cmdMsg()->AddFlat(CHOIR_NAME_PEER_ID, pid).IsOK())&&(cmdMsg()->AddInt32(CHOIR_NAME_NOTE_INDEX, noteIdx).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
   }
}

void ChoirWindow :: AutoScrollMusicSheetWidget(int pixelsToTheRight)
{
   _horizontalScrollBar->setValue(_horizontalScrollBar->value()+pixelsToTheRight);
}

void ChoirWindow :: SeekRequested(uint32 chordIdx)
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_ADJUST_PLAYBACK);
   if ((cmdMsg())&&(cmdMsg()->AddInt32(CHOIR_NAME_CHORD_INDEX, chordIdx).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: SendTransportCommand(uint32 what, const uint32 * optSeekToChordIdx)
{
   UpdateButtons();  // since we only want their state to change when we get an update to our _playbackState, not from a local input
 
   MessageRef cmdMsg = GetMessageFromPool(what);
   if ((cmdMsg())&&((optSeekToChordIdx == NULL)||(cmdMsg()->AddInt32(CHOIR_NAME_CHORD_INDEX, *optSeekToChordIdx).IsOK()))) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: keyPressEvent(QKeyEvent * e)
{
   switch(e->key())
   {
      case Qt::Key_Space:
        if (_playbackState.IsPaused()) PlayButtonClicked();
                                  else PauseButtonClicked();
      break;

      case Qt::Key_Return: case Qt::Key_Enter:
        StopButtonClicked();
      break;

      case Qt::Key_Left:
         if (e->modifiers() & Qt::ShiftModifier) DeleteChord();
                                            else _musicSheetWidget->MoveSeekPosition(-1);
      break;

      case Qt::Key_Right:
         if (e->modifiers() & Qt::ShiftModifier) InsertChord();
                                            else _musicSheetWidget->MoveSeekPosition(1);
      break;

      case Qt::Key_Up:
         TempoChangeRequested(_tempoSlider->value()+_tempoSlider->singleStep());
      break;

      case Qt::Key_Down:
         TempoChangeRequested(_tempoSlider->value()-_tempoSlider->singleStep());
      break;

      case Qt::Key_Delete:
         DeleteChord();
      break;

      case Qt::Key_Insert:
         InsertChord();
      break;

      default:
         QMainWindow::keyPressEvent(e);
      return;
   }

   // If we got here, one of our non-default cases must have handled the keystroke, so accept the event
   e->accept();
}

void ChoirWindow :: UpdateButtons()
{
   const bool isPaused = _playbackState.IsPaused();
   const bool isAtTop  = isPaused ? (_playbackState.GetPausedIndex() == 0) : false;

   _playButton->setChecked(!isPaused);
   _pauseButton->setChecked(isPaused);
   _stopButton->setChecked(!isAtTop);
   _loopButton->setChecked(_playbackState.IsLoop());
   
   _playButton->setEnabled(isPaused);
   _pauseButton->setEnabled(!isPaused);
   _stopButton->setEnabled(!isAtTop);

   if (_userIsDraggingTempoSlider == false) UpdateTempoSliderFromPlaybackState();

   _strategyComboBox->setCurrentIndex(_assigns.GetAssignmentStrategy());
   _rosterWidget->SetReadOnly(_assigns.GetAssignmentStrategy() == ASSIGNMENT_STRATEGY_AUTOMATIC);

   _playMenuItem->setEnabled(!_playButton->isChecked());
   _pauseMenuItem->setEnabled(!_pauseButton->isChecked());
   _stopMenuItem->setEnabled(!_stopButton->isChecked());
   _loopMenuItem->setChecked(_loopButton->isChecked());

   _automaticMenuItem->setChecked(_assigns.GetAssignmentStrategy() == ASSIGNMENT_STRATEGY_AUTOMATIC);
   _assistedMenuItem->setChecked( _assigns.GetAssignmentStrategy() == ASSIGNMENT_STRATEGY_ASSISTED);
   _manualMenuItem->setChecked(   _assigns.GetAssignmentStrategy() == ASSIGNMENT_STRATEGY_MANUAL);
}

void ChoirWindow :: LoopMenuItemSelected()
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_ADJUST_PLAYBACK);
   if ((cmdMsg())&&(cmdMsg()->AddBool(CHOIR_NAME_LOOP, _loopMenuItem->isChecked()).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: LoopButtonClicked()
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_ADJUST_PLAYBACK);
   if ((cmdMsg())&&(cmdMsg()->AddBool(CHOIR_NAME_LOOP, _loopButton->isChecked()).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: UserBeganDraggingTempoSlider()
{
   _userIsDraggingTempoSlider = true;
}

void ChoirWindow :: UpdateTempoSliderFromPlaybackState()
{
   _tempoSlider->setValue(MicrosecondsPerChordToTempo(_tempoSlider, _playbackState.GetMicrosPerChord()));
}

void ChoirWindow :: UserStoppedDraggingTempoSlider()
{
   _userIsDraggingTempoSlider = false;
   UpdateTempoSliderFromPlaybackState();
}

void ChoirWindow :: MusicSheetUpdated()
{
   _musicSheetWidget->SetMusicSheet(DummyConstMusicSheetRef(_musicSheet));  // this will update the GUI
   _rosterWidget->SetNotesUsedInMusicSheet(_musicSheet.GetAllUsedNotesChord());
   ScheduleSendUpdatedMusicSheetToPlayer();
}

void ChoirWindow :: NoteAssignmentsUpdated()
{
   _rosterWidget->SetNoteAssignments(DummyConstNoteAssignmentsMapRef(_assigns));  // causes him to update his GUI

   const uint64 localNotes = _assigns.GetNoteAssignmentsForPeerID(GetLocalPeerID());
   _musicSheetWidget->SetNoteAssignments(localNotes, _assigns.GetAllAssignedNotesChord());  // so he can color-code the notes
   emit LocalNoteAssignmentsChanged(localNotes);  // let Quasimodo know which bells he should ring
}

void ChoirWindow :: MessageReceivedFromServer(const MessageRef & msg, const String & sessionID)
{
   switch(msg()->what)
   {
      case CHOIR_REPLY_GUI_UPDATE:
      {
         // Unwrap the wrapper if necessary
         MessageRef subMsg = msg()->GetMessage(CHOIR_NAME_WRAPPED_MESSAGE);
         if (subMsg()) MessageReceivedFromServer(subMsg, sessionID);
      }
      break;

      case CHOIR_REPLY_LATENCIES_TABLE:
      {
         Hashtable<ZGPeerID, uint64> latencies;
         ZGPeerID pid;
         uint64 latency;
         for (uint32 i=0; ((msg()->FindFlat(CHOIR_NAME_PEER_ID, i, pid).IsOK())&&(msg()->FindInt64(CHOIR_NAME_PEER_LATENCY, i, latency).IsOK())); i++) (void) latencies.Put(pid, latency);
         _rosterWidget->SetLatenciesTable(latencies);
      }
      break;

      case CHOIR_REPLY_NEW_SENIOR_PEER:
      {
         ZGPeerID seniorPeerID; (void) msg()->FindFlat(CHOIR_NAME_PEER_ID, seniorPeerID);
         _rosterWidget->SetSeniorPeerID(seniorPeerID);
      }
      break;

      case CHOIR_COMMAND_SET_CHORD:
         if (_musicSheet.PutChord(msg()->GetInt32(CHOIR_NAME_CHORD_INDEX), msg()->GetInt64(CHOIR_NAME_CHORD_VALUE)).IsOK()) MusicSheetUpdated();
      break;

      case CHOIR_COMMAND_INSERT_CHORD: 
         _musicSheet.InsertChordAt(msg()->GetInt32(CHOIR_NAME_CHORD_INDEX));
         MusicSheetUpdated();
      break;

      case CHOIR_COMMAND_DELETE_CHORD:
         _musicSheet.DeleteChordAt(msg()->GetInt32(CHOIR_NAME_CHORD_INDEX));
         MusicSheetUpdated();
      break;

      case CHOIR_COMMAND_SET_SONG_FILE_PATH:
         _musicSheet.SetSongFilePath(msg()->GetString(CHOIR_NAME_SONG_FILE_PATH));
         MusicSheetUpdated();
      break;

      case CHOIR_COMMAND_TOGGLE_ASSIGNMENT:
         if (_assigns.HandleToggleAssignmentMessage(*msg()).IsOK()) NoteAssignmentsUpdated();
      break;

      case CHOIR_COMMAND_SET_STRATEGY:
         _assigns.SetAssignmentStrategy(msg()->GetInt32(CHOIR_NAME_STRATEGY));
         UpdateButtons();
      break;

      case MUSIC_TYPE_MUSIC_SHEET:
         if (_musicSheet.SetFromArchive(msg).IsOK()) MusicSheetUpdated();
      break;

      case MUSIC_TYPE_PLAYBACK_STATE:
         if (_playbackState.SetFromArchive(msg).IsOK())
         {
            UpdateButtons();
            _musicSheetWidget->SetPlaybackState(_playbackState);
            ScheduleSendUpdatedPlaybackStateToPlayer();
         }
      break;

      case MUSIC_TYPE_ASSIGNMENTS_MAP:
         if (_assigns.SetFromArchive(msg).IsOK()) NoteAssignmentsUpdated();
      break;

      case CHOIR_COMMAND_PEER_ONLINE: case CHOIR_COMMAND_PEER_OFFLINE:
      {
         ZGPeerID peerID;
         if (msg()->FindFlat(CHOIR_NAME_PEER_ID, peerID).IsOK())
         {
            ConstMessageRef optPeerInfo = msg()->GetMessage(CHOIR_NAME_PEER_INFO);
            _rosterWidget->SetPeerIsOnline(peerID, (msg()->what == CHOIR_COMMAND_PEER_ONLINE), optPeerInfo);
            _musicSheetWidget->FullyAttached();  // let him know it's oky to start painting now
         }
      }
      break;
   }
}

void ChoirWindow :: ScheduleSendUpdatedMusicSheetToPlayer()
{
   if (_sendMusicSheetToPlayerPending == false)
   {
      _sendMusicSheetToPlayerPending = true;
      QTimer::singleShot(0, this, SLOT(SendUpdatedMusicSheetToPlayer()));
   }
}

void ChoirWindow :: SendUpdatedMusicSheetToPlayer()
{
   // Note that I make a copy rather than sending a reference to our own _musicSheet
   // because this main thread is likely to modify _musicSheet at any time and I don't
   // want to risk a race condition where MusicPlayer thread is reading the sheet data
   // at the same time this thread is modifying it.
   _sendMusicSheetToPlayerPending = false;
   ConstMusicSheetRef newRef(new MusicSheet(_musicSheet));
   emit SendMusicSheetToPlayer(newRef);
}

void ChoirWindow :: ScheduleSendUpdatedPlaybackStateToPlayer()
{
   if (_sendPlaybackStateToPlayerPending == false)
   {
      _sendPlaybackStateToPlayerPending = true;
      QTimer::singleShot(0, this, SLOT(SendUpdatedPlaybackStateToPlayer()));
   }
}

void ChoirWindow :: SendUpdatedPlaybackStateToPlayer()
{
   _sendPlaybackStateToPlayerPending = false;
   emit SendPlaybackStateToPlayer(_playbackState);
}

status_t ChoirWindow :: UploadMusicSheet(const MusicSheet & musicSheet)
{
   MessageRef uploadMsg = GetMessageFromPool();
   MRETURN_OOM_ON_NULL(uploadMsg());
   MRETURN_ON_ERROR(musicSheet.SaveToArchive(uploadMsg));
   return _serverThread.SendMessageToSessions(uploadMsg);
}

void ChoirWindow :: ClearSong()
{
   if (QMessageBox::question(this, tr("Confirm Clear Song"), tr("Are you sure you want to clear the current song?  This action cannot be undone."), QMessageBox::Ok|QMessageBox::Cancel) == QMessageBox::Ok) (void) UploadMusicSheet(MusicSheet());
}

void ChoirWindow :: OpenSong()
{
   const String & sfp = _musicSheet.GetSongFilePath();

   String executablePath; (void) GetSystemPath(SYSTEM_PATH_EXECUTABLE, executablePath);
#ifdef __APPLE__
   executablePath += "../..";  // get us out of the app package and into the user-visible dir
#endif

   QString openPath = QFileDialog::getOpenFileName(this, tr("Save Music"), sfp.HasChars()?sfp():executablePath(), "*.choirMusic");
   if (openPath.length() > 0)
   {
      QFile f(openPath);
      if (f.open(QIODevice::ReadOnly))
      {
         QByteArray ba = f.readAll();
         Message songMsg;
         if (songMsg.UnflattenFromBytes((const uint8 *) ba.data(), ba.size()).IsOK())
         { 
            MusicSheet temp;
            if (temp.SetFromArchive(DummyConstMessageRef(songMsg)).IsOK())
            {
               temp.SetSongFilePath(openPath.toUtf8().constData());  // update it with our new open-path, just for consistency
               if (UploadMusicSheet(temp).IsOK())
               {
                  LogTime(MUSCLE_LOG_INFO, "Opened song file from [%s]\n", openPath.toUtf8().constData());
                  return;
               }
            }
         }
      }

      // if we got here, something went wrong
      QMessageBox::critical(this, tr("Error opening song file"), tr("Couldn't open song file!"), QMessageBox::Ok);
   }
}

void ChoirWindow :: SaveSong()
{
   QString savePath = QFileDialog::getSaveFileName(this, tr("Save Music"), _musicSheet.GetSongFilePath()(), "*.choirMusic");
   if (savePath.length() > 0)
   {
      MusicSheet temp(_musicSheet);
      temp.SetSongFilePath(savePath.toUtf8().constData());

      Message songMsg;
      if (temp.SaveToArchive(DummyMessageRef(songMsg)).IsOK())
      {
         ConstByteBufferRef msgBytes = songMsg.FlattenToByteBuffer();
         if (msgBytes())
         {
            QFile f(savePath);
            if ((f.open(QIODevice::WriteOnly))&&(f.write((const char *)msgBytes()->GetBuffer(), msgBytes()->GetNumBytes()) == msgBytes()->GetNumBytes())) 
            { 
               LogTime(MUSCLE_LOG_INFO, "Saved song file to [%s]\n", savePath.toUtf8().constData());

               // success!  Now the one other thing we want to do is tell everyone about the new song path
               MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_SET_SONG_FILE_PATH);
               if ((cmdMsg())&&(cmdMsg()->AddString(CHOIR_NAME_SONG_FILE_PATH, savePath.toUtf8().constData()).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
               return;
            }
         }
      }

      // if we got here, something went wrong
      QMessageBox::critical(this, tr("Error saving song file"), tr("Couldn't save song file to disk!"), QMessageBox::Ok);
   }
}

void ChoirWindow :: CloneWindow()
{
   ChoirWindow * newWindow = new ChoirWindow;
   newWindow->show();
}

void ChoirWindow :: InsertChord()
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_INSERT_CHORD);
   if ((cmdMsg())&&(cmdMsg()->AddInt32(CHOIR_NAME_CHORD_INDEX, _musicSheetWidget->GetSeekPointChordIndex()).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

void ChoirWindow :: DeleteChord()
{
   MessageRef cmdMsg = GetMessageFromPool(CHOIR_COMMAND_DELETE_CHORD);
   if ((cmdMsg())&&(cmdMsg()->AddInt32(CHOIR_NAME_CHORD_INDEX, _musicSheetWidget->GetSeekPointChordIndex()).IsOK())) (void) _serverThread.SendMessageToSessions(cmdMsg);
}

}; // end namespace choir

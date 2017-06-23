#include <QMouseEvent>
#include <QPainter>
#include <QFont>
#include "MusicSheetWidget.h"

namespace choir {

static const int _titleHeightPixels = 20;
static const int _chordWidthPixels  = 20;
static const uint32 _topMarginNotes = 1;
static const uint32 _botMarginNotes = 2;
static const uint32 _numVisualNotes = (NUM_CHOIR_NOTES+_topMarginNotes+_botMarginNotes);

static QRgb MixColors(QRgb a, QRgb b) {return a ? ((a&0xFF000000)|(b&0x00FFFFFF)) : 0;}

static QPixmap HighlightNotePixmap(const QPixmap & source, const QColor & c)
{
   QImage tempImage = source.toImage();
   const int w = tempImage.width();
   const int h = tempImage.height();

   const QRgb crgba = c.rgba();
   for (int x=0; x<w; x++)
      for (int y=0; y<h; y++)
         tempImage.setPixel(x, y, MixColors(tempImage.pixel(x,y), crgba));

   return QPixmap::fromImage(tempImage);
}

static QPixmap RotatePixmap180(const QPixmap & pm)
{
   QImage img = pm.toImage(); 
   QPoint center = img.rect().center();
   QMatrix matrix;
   matrix.translate(center.x(), center.y());
   matrix.rotate(180);
   return QPixmap::fromImage(img.transformed(matrix));
}

MusicSheetWidget :: MusicSheetWidget(const INetworkTimeProvider * networkTimeProvider, QWidget * parent) : QWidget(parent), _networkTimeProvider(networkTimeProvider), _ghostChordIndex(MUSCLE_NO_LIMIT), _ghostNoteIndex(MUSCLE_NO_LIMIT), _scrollOffsetX(0), _isFullyAttached(false), _localNotes(0), _allAssignedNotes(0), _seekDraggingIndex(MUSCLE_NO_LIMIT)
{
   _notesAboveStaff = 0; for (uint32 i=CHOIR_NOTE_E6; i<CHOIR_NOTE_F5;   i++) _notesAboveStaff |= (1LL<<i);
   _notesBelowStaff = 0; for (uint32 i=CHOIR_NOTE_C4; i<NUM_CHOIR_NOTES; i++) _notesBelowStaff |= (1LL<<i);

   QPixmap basePixmap(":/quarter_note.png");
   _notePixmaps[NOTE_TYPE_REGULAR][STEM_DIRECTION_UP] = basePixmap;
   _notePixmaps[NOTE_TYPE_LOCAL]  [STEM_DIRECTION_UP] = HighlightNotePixmap(basePixmap, Qt::green);
   _notePixmaps[NOTE_TYPE_ORPHAN] [STEM_DIRECTION_UP] = HighlightNotePixmap(basePixmap, Qt::red);

   for (uint32 i=0; i<NUM_NOTE_TYPES; i++) _notePixmaps[i][STEM_DIRECTION_DOWN] = RotatePixmap180(_notePixmaps[i][STEM_DIRECTION_UP]);

   _animationTimer.setInterval(50);  // 20Hz update rate ought to be plenty
   connect(&_animationTimer, SIGNAL(timeout()), this, SLOT(UpdateWhilePlaying()));

   QFont f = font();
   f.setPixelSize(14);
   setFont(f);

   setMouseTracking(true);
}

MusicSheetWidget :: ~MusicSheetWidget()
{
   // empty
}

int MusicSheetWidget :: GetSongWidthPixels() const
{
   const int rightHandMargin = (_chordWidthPixels*8);  // 8 notes' margin should do fine
   return rightHandMargin + (_musicSheet() ? (_chordWidthPixels*(_musicSheet()->GetSongLengthInChords(false)+1)) : 0);
}

void MusicSheetWidget :: SetMusicSheet(const ConstMusicSheetRef & musicSheet)
{
   _musicSheet = musicSheet;
   emit SongWidthChanged();
   update();
}

static const int _notesToDrawLinesFor[] = {
   CHOIR_NOTE_F5,
   CHOIR_NOTE_D5,
   CHOIR_NOTE_B4,
   CHOIR_NOTE_G4,
   CHOIR_NOTE_E4
};

const QPixmap & MusicSheetWidget :: GetPixmapForNote(uint32 noteIdx, int stemDirection, bool chooseColor, qreal & retOpacity) const
{
   int type = NOTE_TYPE_REGULAR;

   if (chooseColor)
   {
      retOpacity = 1.0;
      if (_localNotes & (1LL<<noteIdx)) type = NOTE_TYPE_LOCAL;
      else
      {
         const uint64 orphanNotes = _musicSheet() ? (_musicSheet()->GetAllUsedNotesChord() & ~_allAssignedNotes) : 0;
         if (orphanNotes & (1LL<<noteIdx)) 
         {
            retOpacity = 0.5;
            type = NOTE_TYPE_ORPHAN;
         }
      }
   }

   return _notePixmaps[type][stemDirection];
}

void MusicSheetWidget :: paintEvent(QPaintEvent * /*event*/)
{
   QPainter p(this);
   p.setPen(Qt::black);

   p.fillRect(QRect(0,0,width(),height()), Qt::white);
   if (_isFullyAttached == false)
   {
      p.drawText(QRect(0,0,width(),height()), Qt::AlignCenter, tr("Joining the choir..."));
      return;
   }

   p.fillRect(QRect(0,0,width(),_titleHeightPixels), Qt::lightGray);

   QString titleStr; 
   if (_musicSheet()) titleStr = _musicSheet()->GetSongFilePath().Substring("/").WithoutSuffix(".choirMusic")();
   if (titleStr.length() == 0) titleStr = "Untitled";
   p.drawText(QRect(0,0,width(),_titleHeightPixels), Qt::AlignCenter, titleStr);

   // Draw chord-marker lines
   uint32 markerChordIndex = GetChordIndexForX(0);
   while(1)
   {
      const int x = GetXForChordIndex(markerChordIndex++, false);
      if (x < width()) p.drawLine(x, _titleHeightPixels, x, _titleHeightPixels-2);
                  else break;
   }

   // Draw horizontal lines
   for (uint32 i=0; i<ARRAYITEMS(_notesToDrawLinesFor); i++)
   {
      const int y = GetYForNote(_notesToDrawLinesFor[i]);
      if (y >= 0) p.drawLine(0,y,width(),y);
   }

   if (_musicSheet())
   {
      if (_scrollOffsetX >= (GetSongWidthPixels()/2))
      {
         // We're closer to the end of the song; draw from the end until we go off the left edge
         for (HashtableIterator<uint32, uint64> iter(_musicSheet()->GetChordsTable(), HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
            if (DrawChord(p, iter.GetKey(), iter.GetValue(), true, GetStemDirectionForChord(iter.GetValue())) < 0) break;
      }
      else
      {
         // We're closer to the front of the song; draw from the beginning until we go off the right edge
         for (HashtableIterator<uint32, uint64> iter(_musicSheet()->GetChordsTable()); iter.HasData(); iter++)
            if (DrawChord(p, iter.GetKey(), iter.GetValue(), true, GetStemDirectionForChord(iter.GetValue())) > 0) break;
      }
   }

   if ((_musicSheet())&&(_ghostChordIndex != MUSCLE_NO_LIMIT)&&(_ghostNoteIndex != MUSCLE_NO_LIMIT)) 
   {
      const uint64 noteBit    = (1LL<<_ghostNoteIndex);
      const uint64 totalChord = noteBit | _musicSheet()->GetChordsTable()[_ghostChordIndex];
      (void) DrawChord(p, _ghostChordIndex, noteBit, false, GetStemDirectionForChord(totalChord), 0.25);
   }

   const int seekPointX = GetSeekPointX();
   if (seekPointX >= 0)
   {
      p.setPen(Qt::red);
      p.setOpacity(0.5f);
      p.drawLine(seekPointX+0, 0, seekPointX+0, height());
      p.drawLine(seekPointX+1, 0, seekPointX+1, height());
   }

   const int64 eofX = GetXForChordIndex(_musicSheet() ? _musicSheet()->GetSongLengthInChords(false) : 0, false);
   p.setPen(Qt::lightGray);
   p.drawLine(eofX, _titleHeightPixels+1, eofX, height()-2);

   p.setPen(Qt::black);
   p.drawRect(QRect(0,0,width()-1,height()-1));
}

int MusicSheetWidget :: GetSeekPointX() const
{
   if (_playbackState.IsPaused()) 
   {
      uint32 pausedIndex = _playbackState.GetPausedIndex();
      if ((_playbackState.IsLoop())&&(_musicSheet())) pausedIndex %= _musicSheet()->GetSongLengthInChords(true);
      return GetXForChordIndex(pausedIndex, _playbackState.IsLoop());
   }
   else
   {
      int64 timeOffset = _networkTimeProvider->GetNetworkTime64()-_playbackState.GetNetworkStartTimeMicros();
      int64 loopTime = ((_musicSheet())&&(_playbackState.IsLoop())) ? (_musicSheet()->GetSongLengthInChords(false)*_playbackState.GetMicrosPerChord()) : 0;
      if (loopTime > 0) timeOffset %= loopTime;
      return GetXForMicrosecondsOffset(timeOffset);
   }
}

void MusicSheetWidget :: leaveEvent(QEvent * e)
{
   QWidget::leaveEvent(e);
   _ghostChordIndex = _ghostNoteIndex = MUSCLE_NO_LIMIT;
   update();
}

void MusicSheetWidget :: mousePressEvent(QMouseEvent * e)
{
   if (e->button() == Qt::LeftButton)
   {
      const uint32 chordIdx = GetChordIndexForX(e->x()); 

      if (e->y() >= _titleHeightPixels)
      {
         const uint32 noteIdx = GetNoteForY(e->y());
         if ((chordIdx != MUSCLE_NO_LIMIT)&&(noteIdx != MUSCLE_NO_LIMIT)) emit NotePositionClicked(chordIdx, noteIdx);
      }
      else 
      {
         _seekDraggingIndex = chordIdx;
         emit SeekRequested(_seekDraggingIndex);
      }
   }
   e->accept();
}

void MusicSheetWidget :: mouseMoveEvent(QMouseEvent * e)
{
   QWidget::mouseMoveEvent(e);

   if (_seekDraggingIndex != MUSCLE_NO_LIMIT)
   {
      uint32 newIdx = GetChordIndexForX(e->x());
      if ((newIdx != _seekDraggingIndex)&&(newIdx != MUSCLE_NO_LIMIT))
      {
         _seekDraggingIndex = newIdx;
         emit SeekRequested(_seekDraggingIndex);
      }
   }
   else
   {
      const uint32 newChordIndex = GetChordIndexForX(e->x());
      const uint32 newNoteIndex  = GetNoteForY(e->y());

      if ((newChordIndex != _ghostChordIndex)||(newNoteIndex != _ghostNoteIndex)) 
      {
         _ghostChordIndex = newChordIndex;
         _ghostNoteIndex  = newNoteIndex;
         update();
      }
   }
}

void MusicSheetWidget :: mouseReleaseEvent(QMouseEvent * e)
{
   QWidget::mouseReleaseEvent(e);
   if (e->button() == Qt::LeftButton) _seekDraggingIndex = MUSCLE_NO_LIMIT;
   e->accept();
}

int MusicSheetWidget :: GetYForNote(uint32 noteIdx) const
{
   if (noteIdx == MUSCLE_NO_LIMIT) return -1;
   return (((noteIdx+_topMarginNotes)*height())/_numVisualNotes)+_titleHeightPixels;
}

uint32 MusicSheetWidget :: GetNoteForY(int y) const
{
   y -= _titleHeightPixels;
   if (y < 0) return MUSCLE_NO_LIMIT;
   return (height()>0) ? muscleClamp((int)(((y*_numVisualNotes)/height())-_topMarginNotes), (int)0, (int)(NUM_CHOIR_NOTES-1)) : 0;
}

int MusicSheetWidget :: GetXForChordIndex(uint32 chordIdx, bool useLoopingLogic) const
{
   if (chordIdx == MUSCLE_NO_LIMIT) return -1;

   if ((useLoopingLogic)&&(_musicSheet())&&(_musicSheet()->GetSongLengthInChords(false))) chordIdx %= _musicSheet()->GetSongLengthInChords(false);
   return ((chordIdx*_chordWidthPixels)-_scrollOffsetX);
}

uint32 MusicSheetWidget :: GetChordIndexForX(int x) const
{
   return (x >= 0) ? ((x+_scrollOffsetX)/_chordWidthPixels) : MUSCLE_NO_LIMIT;
}

int MusicSheetWidget :: GetXForMicrosecondsOffset(int64 microsecondsOffset) const
{
   if (microsecondsOffset < 0) return -1;

   const uint64 microsPerChord = _playbackState.GetMicrosPerChord();
   return (microsPerChord == 0) ? 0 : (((microsecondsOffset*_chordWidthPixels)/microsPerChord)-_scrollOffsetX);
}

static uint32 GetFirstBitIndex(uint64 chord)
{
   for (uint32 i=0; i<(sizeof(chord)*8); i++) if (chord & (1LL<<i)) return i;
   return MUSCLE_NO_LIMIT;
}

static uint32 GetLastBitIndex(uint64 chord)
{
   for (int32 i=(sizeof(chord)*8)-1; i>=0; i--) if (chord & (1LL<<i)) return i;
   return MUSCLE_NO_LIMIT;
}

// Per the rules at:  http://spider.georgetowncollege.edu/music/burnette/notation.htm
// 1. STEM LENGTH:     one octave.
// 2. STEM LENGTH WITH LEGER LINES:  When a note extends beyond one leger line, the stem must touch the middle staff line.
// 3. STEM DIRECTION:  For notes on the middle staff line and above, the stem is down.
// 4. STEM DIRECTION:  For notes below the middle staff line, the stem is up.
// 5. STEM DIRECTION:  When two notes are an equal distance from the middle line, the preferred direction is down.
//                     If the note above is farther from the middle line than the note below, the stem goes down.
//                     If the note below is farther from the middle line, the stem goes up.
// 6. STEM DIRECTION:  When the outer notes (extreme top and bottom notes) are an equal distance from the middle line and the majority of notes are above the middle line, the stem goes down.
// 7. STEM DIRECTION:  When the outer notes are an equal distance from the middle line and the majority of notes are below the middle line, the stem goes up.


int MusicSheetWidget :: GetStemDirectionForChord(uint64 chordVal) const
{
   for (uint32 i=0; i<NUM_CHOIR_NOTES; i++) if (chordVal & (1LL<<i)) return GetStemDirectionForNoteIndex(i);
   return STEM_DIRECTION_UP;
}

int MusicSheetWidget :: DrawChord(QPainter & p, uint32 chordIdx, uint64 chordVal, bool chooseColor, int stemDirection, qreal forceOpacity) const
{
   if ((chordVal & _notesAboveStaff) != 0) DrawNoteLines(p, chordIdx, GetFirstBitIndex(chordVal&~_notesBelowStaff), CHOIR_NOTE_F5, forceOpacity);
   if ((chordVal & _notesBelowStaff) != 0) DrawNoteLines(p, chordIdx, CHOIR_NOTE_C4, GetLastBitIndex(chordVal&~_notesAboveStaff)+1, forceOpacity);

   int ret = 0;
   for (uint32 i=0; i<NUM_CHOIR_NOTES; i++) if (chordVal & (1LL<<i)) ret = DrawNote(p, chordIdx, i, chooseColor, stemDirection, forceOpacity);
   return ret;
}

void MusicSheetWidget :: DrawNoteLines(QPainter & p, uint32 chordIdx, uint32 firstNoteIdx, uint32 afterLastNoteIdx, qreal forceOpacity) const
{
   for (uint32 i=firstNoteIdx; i<afterLastNoteIdx; i++) DrawNoteLine(p, chordIdx, i, forceOpacity);
}

static const int _ledgerLineXMargin = 13;

void MusicSheetWidget :: DrawNoteLine(QPainter & p, uint32 chordIdx, uint32 noteIdx, qreal forceOpacity) const
{
   if (((noteIdx%2)==0)&&((noteIdx < CHOIR_NOTE_F5)||(noteIdx > CHOIR_NOTE_E4)))
   {
      p.setPen(Qt::black);
      p.setOpacity((forceOpacity>=0.0)?forceOpacity:1.0);

      // draw a bit of horizontal line through the bulb of the note, to indicate that it's on a notional line
      QPoint notePt(GetXForChordIndex(chordIdx, false)+(_chordWidthPixels/2), GetYForNote(noteIdx));
      p.drawLine(notePt.x()-_ledgerLineXMargin, notePt.y(), notePt.x()+_ledgerLineXMargin, notePt.y());
   }
}

// returns 0 if the note was drawn, -1 if if it was off the left edge, or +1 if it was off the right edge
int MusicSheetWidget :: DrawNote(QPainter & p, uint32 chordIdx, uint32 noteIdx, bool chooseColor, int stemDirection, qreal forceOpacity) const
{
   if (chordIdx == MUSCLE_NO_LIMIT) return +1;

   const int halfBulbHeight = 6;  // in pixels

   QPoint notePt(GetXForChordIndex(chordIdx, false)+(_chordWidthPixels/2), GetYForNote(noteIdx));
   if (noteIdx != MUSCLE_NO_LIMIT)
   {
      qreal opacity = 1.0;
      const QPixmap & pixmap = GetPixmapForNote(noteIdx, stemDirection, chooseColor, opacity);
      p.setOpacity((forceOpacity>=0.0)?forceOpacity:opacity);
      p.drawPixmap(notePt.x()-(pixmap.width()/2), notePt.y()-((stemDirection==STEM_DIRECTION_DOWN)?halfBulbHeight:(pixmap.height()-halfBulbHeight)), pixmap);
      p.setOpacity(1.0);
   }

   if ((notePt.x()+_ledgerLineXMargin) < 0)        return -1;
   if ((notePt.x()+_ledgerLineXMargin) >= width()) return +1;
   return 0;
}

void MusicSheetWidget :: FullyAttached()
{
   if (_isFullyAttached == false)
   {
      _isFullyAttached = true;
      update();
   }
}

void MusicSheetWidget :: SetPlaybackState(const PlaybackState & newState)
{
   _playbackState = newState;
   update();

   const bool timerShouldFire = !_playbackState.IsPaused();
   if (timerShouldFire != _animationTimer.isActive())
   {
      if (timerShouldFire) _animationTimer.start();
                      else _animationTimer.stop();
   }
}

void MusicSheetWidget :: SetHorizontalScrollOffset(int offsetPixels) 
{
   if (_scrollOffsetX != offsetPixels) 
   {
      _scrollOffsetX = offsetPixels; 
      update();
   }
}

void MusicSheetWidget :: SetNoteAssignments(uint64 localNotes, uint64 allAssignedNotes)
{
   if ((localNotes != _localNotes)||(allAssignedNotes != _allAssignedNotes))
   {
      _localNotes       = localNotes;
      _allAssignedNotes = allAssignedNotes;
      update();
   }
}

void MusicSheetWidget :: UpdateWhilePlaying()
{
   EnsureXIsVisible(GetSeekPointX());
   update();
}

void MusicSheetWidget :: MoveSeekPosition(int delta)
{
   const uint32 curPos = GetSeekPointChordIndex();
   const uint32 songLen = _musicSheet() ? _musicSheet()->GetSongLengthInChords(false) : 0;

   // Allow wrapping-around only if looping is enabled
   int32 newPos;
   if ((songLen>0)&&(_playbackState.IsLoop()))
   {
      newPos = ((int32)curPos)+delta;
      if (newPos < 0) newPos += songLen;
      newPos %= songLen;
   }
   else newPos = muscleClamp((int32)(curPos+delta), (int32)0, (int32)songLen);

   if ((uint32)newPos != curPos) 
   {
      emit SeekRequested(newPos);
      EnsureXIsVisible(GetXForChordIndex(newPos+((delta>0)?1:0), false));
   }
}

void MusicSheetWidget :: EnsureXIsVisible(int x)
{
   const int _hMargin   = 100;
   const int leftX      = _hMargin;
   const int rightX     = width()-_hMargin;

        if (x < leftX)  emit AutoScrollMusicSheetWidget(x-leftX);
   else if (x > rightX) emit AutoScrollMusicSheetWidget(x-rightX);
}

}; // end namespace choir

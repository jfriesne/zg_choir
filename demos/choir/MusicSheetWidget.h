#ifndef MusicSheetWidget_h
#define MusicSheetWidget_h

#include <QPixmap>
#include <QWidget>
#include <QTimer>

#include "zg/INetworkTimeProvider.h"
#include "MusicSheet.h"
#include "PlaybackState.h"

namespace choir {

/** This widget displays a page of our MusicSheet */
class MusicSheetWidget : public QWidget
{
Q_OBJECT

public:
   /** Constructor */
   MusicSheetWidget(const INetworkTimeProvider * networkTimeProvider, QWidget * parent = NULL);

   /** Destructor */
   ~MusicSheetWidget();
   
   /** Sets the MusicSheet object that we should consult to find out where to draw notes at */
   void SetMusicSheet(const ConstMusicSheetRef & musicSheetRef);

   /** Called whenever it's time to redraw this MusicSheetWidget */ 
   virtual void paintEvent(QPaintEvent *);

   /** Called whenever the user presses a mouse button down while the mouse is over this MusicSheetWidget */
   virtual void mousePressEvent(QMouseEvent *);

   /** Called whenever the user moves the mouse across this MusicSheetWidget */
   virtual void mouseMoveEvent(QMouseEvent *);

   /** Called whenever the user releases a mouse button while the mouse is over this MusicSheetWidget */
   virtual void mouseReleaseEvent(QMouseEvent *);

   /** Called whenever the mouse pointer leaves the region of the screen occupied by this MusicSheetWidget */
   virtual void leaveEvent(QEvent *);

   /** Sets the PlaybackState that we should use to animate the red bar */
   void SetPlaybackState(const PlaybackState & newPlaybackState);

   /** Returns the total number of pixels our song should take up horizontally
     * on screen (including a bit of extra space on the right side for appending)
     */
   int GetSongWidthPixels() const;

   /** Returns the number of pixels from the left edge of the song that we have scrolled */
   int GetHorizontalScrollOffset() const {return _scrollOffsetX;}

   /** Should be called when we've become fully attached to the ZG system */
   void FullyAttached();

   /** Sets the bit-chord of notes that we are meant to play, and the bit-chord of all notes in the song
     * @param localNotes a bit-chord of notes that we are meant to play on this peer (i.e. to be rendered in green)
     * @param allAssignedNotes a bit-chord of all the notes that are present in the current song 
     */
   void SetNoteAssignments(uint64 localNotes, uint64 allAssignedNotes);

   /** Called when we want to move the play-position-indicator (vertical red bar) left or right
     * @param delta The number of note-spaces to move.  Positive numbers move it right, negative numbers move it left. 
     */
   void MoveSeekPosition(int delta);

   /** Returns the chord-index that the play-position-indicator (i.e. the vertical red line) is currently at */
   uint32 GetSeekPointChordIndex() const {return GetChordIndexForX(GetSeekPointX());}

signals:
   /** Emitted when the user clicks on a note-position in the music sheet
    *  @param chordIdx the chord-index specifying the click's horizontal position within the score
    *  @param note The CHOIR_NOTE_* value specifying the clicks vertical position on the staff
    */
   void NotePositionClicked(uint32 chordIdx, int note);

   /** Emitted when the user clicks on the seek-row at the top of the music sheet
     * @param chordIdx the chord-index specifying the click's horizontal position within the score
    */
   void SeekRequested(uint32 chordIdx);

   /** Emitted whenever the length of the song (in notes) may have changed */
   void SongWidthChanged();

   /** Emitted when it's time to scroll them music sheet by a relative amount.
     * @param pixelsToTheRight the number of pixels to move the view to the right by.  If negative, then scroll should be to the left.
     */
   void AutoScrollMusicSheetWidget(int pixelsToTheRight);

public slots:
   /** Sets the horizontal-scroll-offset of this widget.  All graphics will be shifted horizontally by this much.
     * @param offsetPixels the new scroll-offset, in pixels.
     */
   void SetHorizontalScrollOffset(int offsetPixels);

private slots:
   void UpdateWhilePlaying();

private:
   int GetStemDirectionForNoteIndex(uint32 noteIdx) const {return (noteIdx<=CHOIR_NOTE_B4)?STEM_DIRECTION_DOWN:STEM_DIRECTION_UP;}
   int GetStemDirectionForChord(uint64 chordVal) const;

   int GetYForNote(uint32 noteIdx) const;
   uint32 GetNoteForY(int y) const;

   int GetXForChordIndex(uint32 chordIdx, bool useLoopingLogic) const;
   uint32 GetChordIndexForX(int x) const;

   int GetXForMicrosecondsOffset(int64 microsecondsOffset) const;

   int DrawChord(QPainter & p, uint32 chordIdx, uint64 chordVal, bool chooseNoteColor, int stemDirection, qreal optForceOpacity = -1.0) const;
   int DrawNote(QPainter & p, uint32 chordIdx, uint32 noteIdx, bool chooseNoteColor, int stemDirection, qreal optForceOpacity = -1.0) const;
   void DrawNoteLine(QPainter & p, uint32 chordIdx, uint32 noteIdx, qreal optForceOpacity = -1.0) const;
   void DrawNoteLines(QPainter & p, uint32 chordIdx, uint32 firstNoteIdx, uint32 afterLastNoteIdx, qreal optForceOpacity = -1.0) const;
   int GetSeekPointX() const;
   void EnsureXIsVisible(int x);

   const QPixmap & GetPixmapForNote(uint32 noteIdx, int stemDirection, bool chooseColor, qreal & retOpacity) const;

   const INetworkTimeProvider * _networkTimeProvider;

   ConstMusicSheetRef _musicSheet;
   PlaybackState _playbackState;

   QTimer _animationTimer;

   enum {
      STEM_DIRECTION_UP = 0,
      STEM_DIRECTION_DOWN,
      NUM_STEM_DIRECTIONS
   };

   enum {
      NOTE_TYPE_REGULAR = 0,
      NOTE_TYPE_LOCAL,
      NOTE_TYPE_ORPHAN,
      NUM_NOTE_TYPES
   };

   QPixmap _notePixmaps[NUM_NOTE_TYPES][NUM_STEM_DIRECTIONS];

   uint32 _ghostChordIndex;
   uint32 _ghostNoteIndex;

   int _scrollOffsetX;

   bool _isFullyAttached;

   uint64 _localNotes;       // bit-chord of notes that we will be playing locally
   uint64 _allAssignedNotes; // bit-chord of notes that somebody will be playing

   uint32 _seekDraggingIndex;  // MUSCLE_NO_LIMIT when not dragging on the seek-bar

   uint64 _notesBelowStaff;  // bit-chord of notes in the white area below the horizontal lines
   uint64 _notesAboveStaff;  // bit-chord of notes in the white area above the horizontal lines
};

}; // end namespace choir

#endif

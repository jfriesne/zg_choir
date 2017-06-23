#ifndef RosterWidget_h
#define RosterWidget_h

#include <QPixmap>
#include <QWidget>
#include <QTimer>

#include "zg/INetworkTimeProvider.h"
#include "zg/ZGPeerID.h"
#include "NoteAssignmentsMap.h"

namespace choir {

/** This widget displays the current table of note assignments, as contained in our NoteAssignmentsMap */
class RosterWidget : public QWidget
{
Q_OBJECT

public:
   /** Constructor */
   RosterWidget(const ZGPeerID & localPeerID, QWidget * parent = NULL);

   /** Destructor */
   ~RosterWidget();
   
   /** Called when the RosterWidget first appears on screen. */
   virtual void showEvent(QShowEvent *);

   /** Called when it's time to repaint the RosterWidget */
   virtual void paintEvent(QPaintEvent *);

   /** Called when the user presses down on the mouse button while over the RosterWidget */
   virtual void mousePressEvent(QMouseEvent *);

   /** Called when the user drags the mouse on the RosterWidget */
   virtual void mouseMoveEvent(QMouseEvent *);

   /** Called when the user releases the mouse button while over the RosterWidget */
   virtual void mouseReleaseEvent(QMouseEvent *);

   /** Called when the mouse pointer exits the RosterWidget's on-screen area */
   virtual void leaveEvent(QEvent *);

   /** Called when the user turns the mouse-wheel while the mouse is over the RosterWidget */
   virtual void wheelEvent(QWheelEvent *);

   /** Called to tell the RosterWidget which notes are used in the current song.
     * The RosterWidget will use this information to lay out the columns it displays.
     * @param notesChord a bit-chord of CHOIR_NOTE_* values
     */
   void SetNotesUsedInMusicSheet(uint64 notesChord);

   /** Called when a peer comes online or goes offline.
     * The RosterWidget will use this information to lay out the rows it displays.
     * @param peerID the ZGPeerID of the peer we are being notified about
     * @param isOnline True iff this peer just came online; false iff this peer just went offline. 
     * @param optPeerInfo Reference to some information about the peer (e.g. its on-screen name)
     */
   void SetPeerIsOnline(const ZGPeerID & peerID, bool isOnline, const ConstMessageRef & optPeerInfo);

   /** Called whenever the senior-peer of the system changes.
     * The RosterWidget uses this information to render the senior peer's name in italics.
     * @param pid the ZGPeerID of the new senior peer, or an invalid ZGPeerID if there is no senior peer(!?)
     */
   void SetSeniorPeerID(const ZGPeerID & pid);

   /** Called to set the NotesAssignmentMap that the RosterWidget should consult to render handbell icons.
     * TheRosterWidget will place handbell icons in the grid according to the state of this object.
     * @param notesRef a reference to a NotesAssignmentMap object.
     */
   void SetNoteAssignments(const ConstNoteAssignmentsMapRef & notesRef) {_assigns = notesRef; update();}

   /** Returns how many pixels we have scrolled down from the top of the view. */
   int GetVerticalScrollOffset() const {return _scrollOffsetY;}

   /** Returns The total height of the view's contents. */
   int GetTotalContentHeight() const;

   /** Set whether this widget should allow user input via mouse-clicks or not.
     * @param ro Iff true, this view will become read-only (darkened and non-clickable)
     */
   void SetReadOnly(bool ro) {if (_readOnly != ro) {_readOnly = ro; update();}}

   /** Returns true iff this view is currently in read-only mode */
   bool IsReadOnl() const {return _readOnly;}

signals:
   /** Emitted whenever a cell in the grid is clicked on by the user
     * @param peerID the ZGPeerID corresponding to the row that was clicked on
     * @param noteIdx The CHOIR_NOTE_* value corresponding to the column that was clicked on
     */
   void BellPositionClicked(const zg::ZGPeerID & peerID, uint32 noteIdx);

   /** Emitted whenever rows are added or removed from this view's grid */
   void TotalContentHeightChanged();

   /** Emitted whenever the user turns his little mouse-wheel while hovering over this view
     * @param dy A value indicating how far and in what direction the mouse-wheel was turned.
     */
   void WheelTurned(int dy);

public slots:
   /** Sets our vertical-scroll offset
     * @param vso The number of pixels down from the top of the grid we should be scrolled to.
     */
   void SetVerticalScrollOffset(int vso) {_scrollOffsetY = vso; update();}

   /** Causes the bells specified in the bit-chord to visually vibrate, as if they had been struck.
     * @param notesChord a bit-chord of CHOIR_NOTE_* values indicating which bells to vibrate.
     */
   void AnimateLocalBells(quint64 notesChord);

   /** Sets the latency values we should display to the user at the right-hand side of the grid.
     * @param latenciesTable a table of ZGPeerIDs and their associated latencies (in microseconds)
     */
   void SetLatenciesTable(Hashtable<ZGPeerID, uint64> & latenciesTable);

private slots:
   void ClearAnimatedLocalBells();

private:
   QString GetPeerNickname(const ZGPeerID & pid) const;

   int GetYForRow(uint32 rowIdx) const;
   uint32 GetRowForY(int y) const;

   int GetXForColumn(uint32 colIdx) const;
   uint32 GetColumnForX(int x) const;

   uint32 GetRowForPeerID(const ZGPeerID & pid) const {return (uint32) _onlinePeers.IndexOfKey(pid);}
   const ZGPeerID & GetPeerIDForRow(uint32 row) const {return _onlinePeers.GetKeyAtWithDefault(row);}

   uint32 GetColumnForNoteIndex(uint32 noteIdx) const {return _noteIndexToColumnIndex.GetWithDefault(noteIdx, MUSCLE_NO_LIMIT);}
   uint32 GetNoteIndexForColumn(uint32 colIdx)  const {return _columnIndexToNoteIndex.GetWithDefault(colIdx,  MUSCLE_NO_LIMIT);}

   int GetYForPeerID(const ZGPeerID & peerID) const {return GetYForRow(GetRowForPeerID(peerID));}
   const ZGPeerID & GetPeerIDForY(int y)      const {return GetPeerIDForRow(GetRowForY(y));}

   int GetXForNoteIndex(uint32 noteIndex) const {return GetXForColumn(GetColumnForNoteIndex(noteIndex));}
   uint32 GetNoteIndexForX(int x)         const {return GetNoteIndexForColumn(GetColumnForX(x));}

   void DrawShadedRow(QPainter & p, const ZGPeerID & peerID, const QColor & c);
   void DrawBell(QPainter & p, uint32 rowIdx, uint32 colIdx, bool shakeIt) const;
   void DrawBellAt(QPainter & p, int x, int y) const;

   void HandleMouseEvent(QMouseEvent * e, bool isPress);

   const ZGPeerID _localPeerID;
   const QPixmap _bellPixmap;

   OrderedKeysHashtable<ZGPeerID, ConstMessageRef> _onlinePeers;  // the rows we are currently showing
   Hashtable<ZGPeerID, uint64> _peerNoteAssignments;   // which peers should be doing which notes

   // our current column<->noteIndex mapping
   uint64 _currentNotesChord;
   Queue<uint32> _columnIndexToNoteIndex;
   Hashtable<uint32, uint32> _noteIndexToColumnIndex;

   ConstNoteAssignmentsMapRef _assigns;
   int _scrollOffsetY;

   QString _noteNames[NUM_CHOIR_NOTES];
   int _draggingNoteIdx;
   int _draggingNoteY;       // widget-coords: only valid if (_draggingNoteIdx>=0)
   int _draggingNoteYStart;  // widget-coords: where the drag started
   int _draggingNoteYOffset; // difference between where the user started the drag, and the middle of the cell

   bool _readOnly;
   uint64 _animatedBells;

   Hashtable<ZGPeerID, uint64> _peerIDToLatency;  // updated at a constant rate, just to make the GUI look better
   ZGPeerID _seniorPeerID;

   QFont _drawFont;
   QFont _italicizedFont;
};

}; // end namespace choir

#endif

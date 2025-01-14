#include <QMouseEvent>
#include <QPainter>
#include <QFont>

#include "RosterWidget.h"
#include "util/TimeUtilityFunctions.h"

namespace choir {

static const int _topHeaderHeight = 20;
static const int _leftHeaderWidth = 100;
static const int _rowHeight       = 30;
static const int _columnWidth     = 30;
static const int _bellIconMargin  = 4;

RosterWidget :: RosterWidget(const ZGPeerID & localPeerID, QWidget * parent)
   : QWidget(parent)
   , _localPeerID(localPeerID)
   , _bellPixmap(QPixmap(":/choir_bell.png").scaled(QSize(_columnWidth-(_bellIconMargin*2), _rowHeight-(_bellIconMargin*2)), Qt::IgnoreAspectRatio, Qt::SmoothTransformation))
   , _ringingBellPixmap(QPixmap(":/choir_bell_ringing.png").scaled(QSize(_columnWidth-(_bellIconMargin*2), _rowHeight-(_bellIconMargin*2)), Qt::IgnoreAspectRatio, Qt::SmoothTransformation))
   , _currentNotesChord(0)
   , _scrollOffsetY(0)
   , _draggingNoteIdx(-1)
   , _readOnly(false)
   , _animatedBells(0)
{
   for (uint32 i=0; i<ARRAYITEMS(_noteNames); i++) _noteNames[i] = GetNoteName(i);
   (void) _onlinePeers.PutWithDefault(ZGPeerID());  // the invalid ZGPeerID will represent our shelf of unassigned bells
}

RosterWidget :: ~RosterWidget()
{
   // empty
}

void RosterWidget :: SetNotesUsedInMusicSheet(uint64 notesChord)
{
   if (notesChord != _currentNotesChord)
   {
      _currentNotesChord = notesChord;

      // Recompute our columns-layout-indices based on the new set of notes-in-use
      _columnIndexToNoteIndex.Clear();
      _noteIndexToColumnIndex.Clear();
      for (int32 i=NUM_CHOIR_NOTES-1; i>=0; i--)
      {
         if ((_currentNotesChord & (1LL<<i)) != 0)
         {
            (void) _columnIndexToNoteIndex.AddTail(i);
            (void) _noteIndexToColumnIndex.Put(i, _columnIndexToNoteIndex.GetNumItems()-1);
         }
      }

      update();
   }
}

void RosterWidget :: SetPeerIsOnline(const ZGPeerID & peerID, bool isOnline, const ConstMessageRef & optPeerInfo)
{
   if (isOnline) (void) _onlinePeers.Put(peerID, optPeerInfo);
            else (void) _onlinePeers.Remove(peerID);
   update();
   emit TotalContentHeightChanged();
}

int RosterWidget :: GetYForRow(uint32 rowIdx) const
{
   return (rowIdx == MUSCLE_NO_LIMIT) ? -1 : (_topHeaderHeight+(rowIdx*_rowHeight));
}

uint32 RosterWidget :: GetRowForY(int y) const
{
   y -= _topHeaderHeight;
   return muscleInRange(y, 0, (int)((_rowHeight*_onlinePeers.GetNumItems())-1)) ? (y/_rowHeight) : MUSCLE_NO_LIMIT;
}

int RosterWidget :: GetXForColumn(uint32 colIdx) const
{
   return (colIdx == MUSCLE_NO_LIMIT) ? width() : (_leftHeaderWidth+(colIdx*_columnWidth));
}

uint32 RosterWidget :: GetColumnForX(int x) const
{
   x -= _leftHeaderWidth;
   return muscleInRange(x, 0, width()-1) ? (x/_columnWidth) : MUSCLE_NO_LIMIT;
}

QString RosterWidget :: GetPeerNickname(const ZGPeerID & pid) const
{
   return pid.IsValid() ? choir::GetPeerNickname(pid, _onlinePeers[pid])() : tr("Unassigned");
}

void RosterWidget :: DrawShadedRow(QPainter & p, const ZGPeerID & peerID, const QColor & c)
{
   uint32 rowIdx = GetRowForPeerID(peerID);
   if (rowIdx != MUSCLE_NO_LIMIT)
   {
      QColor cc = c; cc.setAlpha(128);
      const int colsRightX  = GetXForColumn(_columnIndexToNoteIndex.GetNumItems());
      p.fillRect(QRect(_leftHeaderWidth, GetYForRow(rowIdx), colsRightX-_leftHeaderWidth, _rowHeight), cc);
   }
}

int RosterWidget :: GetTotalContentHeight() const
{
   return _topHeaderHeight + (_onlinePeers.GetNumItems()*_rowHeight);
}

void RosterWidget :: showEvent(QShowEvent * e)
{
   QWidget::showEvent(e);

   _drawFont = font();
   _italicizedFont = _drawFont;
   _italicizedFont.setItalic(true);
}

void RosterWidget :: paintEvent(QPaintEvent * /*event*/)
{
   if (_onlinePeers.GetNumItems() <= 1) return;  // we'll be fully attached when at least one non-default peer shows up

   const int colsRightX  = GetXForColumn(_columnIndexToNoteIndex.GetNumItems());
   const int rowsBottomY = GetYForRow(_onlinePeers.GetNumItems());
   const int chartHeight = _onlinePeers.GetNumItems()*_rowHeight;

   QPainter p(this);
   p.translate(0, -_scrollOffsetY);
   p.setPen(Qt::black);
   p.fillRect(QRect(_leftHeaderWidth,_topHeaderHeight,width()-_leftHeaderWidth,chartHeight), Qt::gray);
   p.fillRect(QRect(_leftHeaderWidth,_topHeaderHeight,colsRightX-_leftHeaderWidth,rowsBottomY-_topHeaderHeight), Qt::black);
   DrawShadedRow(p, ZGPeerID(),   Qt::red);
   DrawShadedRow(p, _localPeerID, Qt::green);
   p.drawRect(_leftHeaderWidth-1,_topHeaderHeight-1,width()-(_leftHeaderWidth+1),chartHeight+2);

   // Draw out the row lines
   if (_onlinePeers.HasItems())
   {
      uint64 assignedChord = 0;

      uint32 rowIdx = 0;
      for (HashtableIterator<ZGPeerID, ConstMessageRef> iter(_onlinePeers); iter.HasData(); iter++)
      {
         const ZGPeerID & peerID = iter.GetKey();
         const int y = GetYForRow(rowIdx);

         // Draw client names
         const uint64 latencyMicros = _peerIDToLatency.GetWithDefault(peerID, MUSCLE_TIME_NEVER);
         p.setPen(Qt::black);
         p.setFont((peerID==_seniorPeerID)?_italicizedFont:_drawFont);
         p.drawText(QRect(0, y, _leftHeaderWidth, _rowHeight), Qt::AlignCenter, GetPeerNickname(peerID));
         p.setFont(_drawFont);
         p.drawText(QRect(0, y, width()-5, _rowHeight), Qt::AlignRight|Qt::AlignVCenter, peerID.IsValid()?((latencyMicros==MUSCLE_TIME_NEVER)?tr("???"):tr("%1 mS").arg(MicrosToMillis(latencyMicros))):tr("Latency"));

         if (_columnIndexToNoteIndex.HasItems())
         {
            p.setPen(Qt::white);
            p.drawLine(_leftHeaderWidth, y, colsRightX, y);

            if (peerID.IsValid())
            {
               const uint64 chord = _assigns() ? _assigns()->GetNoteAssignmentsForPeerID(peerID) : 0;
               if (chord != 0)
               {
                  const uint64 animatedBells = (peerID == _localPeerID) ? _animatedBells : 0;
                  assignedChord |= chord;
                  for (uint32 i=0; i<NUM_CHOIR_NOTES; i++) if (chord & (1LL<<i)) DrawBell(p, rowIdx, GetColumnForNoteIndex(i), ((animatedBells&(1LL<<i))!=0));
               }
            }
         }

         rowIdx++;
      }

      // Any notes that we didn't display a bell for, we'll display in the unassigned-row
      for (uint32 i=0; i<NUM_CHOIR_NOTES; i++) if ((((int32)i)!=_draggingNoteIdx)&&((assignedChord & (1LL<<i)) == 0)) DrawBell(p, 0, GetColumnForNoteIndex(i), false);

      if (_columnIndexToNoteIndex.HasItems())
      {
         p.setPen(Qt::white);
         p.drawLine(_leftHeaderWidth, rowsBottomY, colsRightX, rowsBottomY);
      }
   }

   // Draw out the column lines
   if (_columnIndexToNoteIndex.HasItems())
   {
      for (uint32 colIdx=0; colIdx<_columnIndexToNoteIndex.GetNumItems(); colIdx++)
      {
         const int x = GetXForColumn(colIdx);

         p.setPen(Qt::black);
         p.drawText(QRect(x, 0, _columnWidth, _topHeaderHeight), Qt::AlignCenter, _noteNames[GetNoteIndexForColumn(colIdx)]);

         p.setPen(Qt::white);
         p.drawLine(x, _topHeaderHeight, x, rowsBottomY);
      }
      p.setPen(Qt::white);
      p.drawLine(colsRightX, _topHeaderHeight, colsRightX, rowsBottomY);
   }

   if (_draggingNoteIdx >= 0)
   {
      p.setOpacity(0.5);
      DrawBellAt(p, GetXForColumn(GetColumnForNoteIndex(_draggingNoteIdx)), _scrollOffsetY+_draggingNoteY-_draggingNoteYOffset-(_bellPixmap.height()/2), false);
      p.setOpacity(1.0);
   }

   if (_readOnly)
   {
      p.translate(0, _scrollOffsetY);  // undo the original translation
      p.fillRect(QRect(0,0,width(),height()), QColor(100,100,100,100));
   }
}

void RosterWidget :: wheelEvent(QWheelEvent * e)
{
   QWidget::wheelEvent(e);
   emit WheelTurned(e->angleDelta().y());
   e->accept();
}

void RosterWidget :: leaveEvent(QEvent * e)
{
   QWidget::leaveEvent(e);
}

void RosterWidget :: mousePressEvent(QMouseEvent * e)
{
   if (_readOnly) {e->ignore(); return;}

   QWidget::mousePressEvent(e);
   if (e->button() == Qt::LeftButton) HandleMouseEvent(e, true);
   e->accept();
}

void RosterWidget :: mouseMoveEvent(QMouseEvent * e)
{
   if (_readOnly) {e->ignore(); return;}

   QWidget::mouseMoveEvent(e);
   if (_draggingNoteIdx >= 0)
   {
      _draggingNoteY = e->pos().y();
      update();
   }
   e->accept();
}

void RosterWidget :: mouseReleaseEvent(QMouseEvent * e)
{
   if (_readOnly) {e->ignore(); return;}

   QWidget::mouseReleaseEvent(e);
   if ((e->button() == Qt::LeftButton)&&(_draggingNoteIdx >= 0))
   {
      HandleMouseEvent(e, false);
      _draggingNoteIdx = -1;
      update();
   }
   e->accept();
}

void RosterWidget :: HandleMouseEvent(QMouseEvent * e, bool isPress)
{
   const QPoint p = e->pos();
   const int logicalY = p.y()+_scrollOffsetY;
   const uint32 rowIdx = GetRowForY(logicalY);
   if (rowIdx < _onlinePeers.GetNumItems())
   {
      const uint32 noteIdx = (_draggingNoteIdx >= 0) ? _draggingNoteIdx : GetNoteIndexForX(p.x());
      if (noteIdx != MUSCLE_NO_LIMIT)
      {
         const uint64 chord     = _assigns()  ? _assigns()->GetNoteAssignmentsForPeerID(GetPeerIDForRow(rowIdx)) : 0;
         const bool cellHasBell = (rowIdx==0) ? (chord == 0) : (chord & (1LL<<noteIdx));
         bool emitSignal = false;

         if (isPress)
         {
            if (cellHasBell)
            {
               _draggingNoteIdx     = noteIdx;
               _draggingNoteY       = p.y();
               _draggingNoteYStart  = p.y();
               _draggingNoteYOffset = (logicalY-(GetYForRow(rowIdx)+(_rowHeight/2)));
            }
            emitSignal = true;
         }
         else emitSignal = ((!cellHasBell)&&(muscleAbs(p.y()-_draggingNoteYStart)>5));

         if (emitSignal) emit BellPositionClicked(GetPeerIDForRow(rowIdx), (_draggingNoteIdx>=0)?_draggingNoteIdx:noteIdx);  // remove the existing bell or add one
      }
   }
}

void RosterWidget :: DrawBell(QPainter & p, uint32 rowIdx, uint32 colIdx, bool shakeIt) const
{
   if ((rowIdx == MUSCLE_NO_LIMIT)||(colIdx == MUSCLE_NO_LIMIT)) return;  // paranoia
   DrawBellAt(p, GetXForColumn(colIdx)+(shakeIt?2:0), GetYForRow(rowIdx)+(shakeIt?2:0), shakeIt);
}

void RosterWidget :: DrawBellAt(QPainter & p, int x, int y, bool ringing) const
{
   p.drawPixmap(x+_bellIconMargin, y+_bellIconMargin, ringing?_ringingBellPixmap:_bellPixmap);
}

void RosterWidget :: AnimateLocalBells(quint64 notesChord)
{
   _animatedBells |= notesChord;
   update();

   QTimer::singleShot(100, this, SLOT(ClearAnimatedLocalBells()));
}

void RosterWidget :: ClearAnimatedLocalBells()
{
   if (_animatedBells != 0)
   {
      _animatedBells = 0;
      update();
   }
}

void RosterWidget :: SetLatenciesTable(Hashtable<ZGPeerID, uint64> & table)
{
   _peerIDToLatency.SwapContents(table);
   update();
}

void RosterWidget :: SetSeniorPeerID(const ZGPeerID & newSeniorPeerID)
{
   if (newSeniorPeerID != _seniorPeerID)
   {
      _seniorPeerID = newSeniorPeerID;
      update();
   }
}

}; // end namespace choir

#include <QPainter>
#include <QMouseEvent>
#include <QWidget>

#include "FridgeClientCanvas.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)

namespace fridge {

static const char * _magnetWordsList[] = {
   "Foo",
   "Bar",
   "Baz",
   "Blah",
   "Nerd",
   "Blorf"
};

FridgeClientCanvas :: FridgeClientCanvas(ITreeGateway * connector) 
   : ITreeGatewaySubscriber(connector)
   , _nextMagnetWordIndex(0)
{
   (void) AddTreeSubscription("magnets/*");

   (void) _magnetWords.EnsureSize(ARRAYITEMS(_magnetWordsList));
   for (uint32 i=0; i<ARRAYITEMS(_magnetWordsList); i++) (void) _magnetWords.AddTail(_magnetWordsList[i]);

   // shuffle the deck
   srand(time(NULL));
   for (uint32 i=0; i<_magnetWords.GetNumItems(); i++)
   {
      const uint32 randIdx = rand()%_magnetWords.GetNumItems();
      if (i != randIdx)
      {
         String & s1 = _magnetWords[i];
         String & s2 = _magnetWords[randIdx];
         String temp = s1;
         s1 = s2;
         s2 = temp;
      }
   }
}

FridgeClientCanvas :: ~FridgeClientCanvas()
{
   // empty
}

void FridgeClientCanvas :: paintEvent(QPaintEvent *)
{
   QPainter p(this);
   QFontMetrics fm = p.fontMetrics();

   p.fillRect(rect(), Qt::lightGray);
   for (HashtableIterator<String, MagnetState> iter(_magnets); iter.HasData(); iter++)
   {
      const MagnetState & m = iter.GetValue();
      m.Draw(p, m.GetScreenRect(fm));
   }
}

String FridgeClientCanvas :: GetMagnetAtPoint(const QPoint & pt) const
{
   QFontMetrics fm = fontMetrics();
   for (HashtableIterator<String, MagnetState> iter(_magnets); iter.HasData(); iter++)
      if (iter.GetValue().GetScreenRect(fm).contains(pt)) return iter.GetKey();
   return GetEmptyString();
}

void FridgeClientCanvas :: mousePressEvent(QMouseEvent * e)
{
   String clickedOn = GetMagnetAtPoint(e->pos());
   if (clickedOn.HasChars())
   {
      const Point & ulp = _magnets[clickedOn].GetUpperLeftPos();
      _draggingID = clickedOn;
      _dragDelta  = QPoint(e->x()-ulp.x(), e->y()-ulp.y());
   }
   else 
   {
      status_t ret;
      MagnetState newMagnet(Point(e->x(), e->y()), GetNextMagnetWord());
      if (UploadMagnetState(GetEmptyString(), &newMagnet).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't upload new magnet, error [%s]\n", ret());
   }

   e->accept(); 
}

void FridgeClientCanvas :: mouseMoveEvent(QMouseEvent * e)
{
   if (_draggingID.HasChars()) UpdateDraggedMagnetPosition(e->pos());
   e->accept();
}

void FridgeClientCanvas :: mouseReleaseEvent(QMouseEvent * e)
{
   if (_draggingID.HasChars())
   {
      UpdateDraggedMagnetPosition(e->pos());
      _draggingID.Clear();
   }

   e->accept();
}

void FridgeClientCanvas :: leaveEvent(QEvent * e)
{
   if (_draggingID.HasChars())
   {
      MagnetState * ms = _magnets.Get(_draggingID);
      if (ms)
      {
         status_t ret;
         if (UploadMagnetState(_draggingID, NULL).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't remove deleted magnet, error [%s]\n", ret());
      }
      _draggingID.Clear();
   }

   e->accept();
}

void FridgeClientCanvas :: UpdateDraggedMagnetPosition(QPoint mousePos)
{
   const MagnetState * ms = _magnets.Get(_draggingID);
   if (ms)
   {
      const QPoint upperLeftPos = (mousePos-_dragDelta);
      MagnetState newState(*ms);
      newState.SetUpperLeftPos(Point(upperLeftPos.x(), upperLeftPos.y()));

      status_t ret;
      if (UploadMagnetState(_draggingID, &newState).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't upload moved magnet, error [%s]\n", ret());
   }
}

status_t FridgeClientCanvas :: UploadMagnetState(const String & optNodeID, const MagnetState * optMagnetState)
{
   MessageRef msgRef;

   if (optMagnetState)
   {
      msgRef  = GetMessageFromPool();
      if (msgRef() == NULL) RETURN_OUT_OF_MEMORY;

      status_t ret;
      if (optMagnetState->SaveToArchive(*msgRef()).IsError(ret)) return ret;
   }
      
   return UploadTreeNodeValue(optNodeID.Prepend("magnets/"), msgRef);
}

void FridgeClientCanvas :: TreeGatewayConnectionStateChanged()
{
   ITreeGatewaySubscriber::TreeGatewayConnectionStateChanged();
   if ((IsTreeGatewayConnected() == false)&&(_magnets.HasItems()))
   {
      _magnets.Clear();
      update();
   }
}

void FridgeClientCanvas :: TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg)
{
   if (nodePath.StartsWith("magnets/"))
   {
      const String nodeName = nodePath.Substring(8);
      if (optPayloadMsg())
      {
         MagnetState state;
         if ((state.SetFromArchive(*optPayloadMsg()).IsOK())&&(_magnets.Put(nodeName, state).IsOK())) update();
      }
      else if (_magnets.Remove(nodeName).IsOK()) update();
   }
}


String FridgeClientCanvas :: GetNextMagnetWord()
{
   const String ret = _magnetWords[_nextMagnetWordIndex];
   _nextMagnetWordIndex = (_nextMagnetWordIndex+1)%_magnetWords.GetNumItems();
   return ret;
}

}; // end namespace fridge

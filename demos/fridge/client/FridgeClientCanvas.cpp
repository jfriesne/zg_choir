#include <QPainter>
#include <QMouseEvent>
#include <QWidget>

#include "FridgeClientCanvas.h"
#include "common/FridgeConstants.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)
#include "zg/messagetree/gateway/SymlinkLogicMuxTreeGateway.h"  // for SYMLINK_FIELD_NAME

namespace fridge {

FridgeClientCanvas :: FridgeClientCanvas(ITreeGateway * connector) 
   : ITreeGatewaySubscriber(connector)
   , _firstMouseMove(false)
{
   (void) AddTreeSubscription("project/magnets/*");    // we need to keep track of where the magnets are on the server
   (void) AddTreeSubscription("project/last_dragged"); // test out symlink functionality by subscribing to this node that is actually a "symlink"
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

   if (_lastDraggedMagnet.GetText().HasChars())
   {
      const int margin = 4;
      QPen thePen(Qt::green);
      thePen.setWidth(margin+2);
      p.setPen(thePen);
      p.setBrush(Qt::NoBrush);
      p.drawRect(_lastDraggedMagnet.GetScreenRect(fontMetrics()).marginsAdded(QMargins(margin,margin,margin,margin)));
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
   const QPoint p = e->pos();

   const String clickedOn = GetMagnetAtPoint(e->pos());
   if (clickedOn.HasChars())
   {
      const MagnetState & magnet = _magnets[clickedOn];

      const Point & ulp = magnet.GetUpperLeftPos();
      _draggingID     = clickedOn;
      _dragDelta      = QPoint(p.x()-ulp.x(), p.y()-ulp.y());
      _firstMouseMove = true;

      (void) BeginUndoSequence(String("Move Magnet \"%1\"").Arg(magnet.GetText()));

      // Experimental:  Set a symlink node to point to the node we are currently dragging
      MessageRef symlinkMsg = GetMessageFromPool();
      if ((symlinkMsg())&&(symlinkMsg()->AddString(SYMLINK_FIELD_NAME, _draggingID.Prepend("project/magnets/")).IsOK())) (void) UploadTreeNodeValue("project/last_dragged", symlinkMsg);
   }
   else 
   {
      // Just to exercise the subscriber<->seniorpeer message-passing functionality, let's 
      // ask the senior peer to return a random word for us to use, rather than generating 
      // the word  ourself.
      MessageRef requestMsg = GetMessageFromPool(FRIDGE_COMMAND_GETRANDOMWORD);
      if ((requestMsg())&&(requestMsg()->AddPoint("pos", Point(p.x(), p.y())).IsOK()))
      {
         const status_t ret = SendMessageToTreeSeniorPeer(requestMsg);
         if (ret.IsError()) LogTime(MUSCLE_LOG_ERROR, "Error, SendMessageToTreeSeniorPeer() failed!  [%s]\n", ret());
      }
   }
   e->accept(); 
}
      
void FridgeClientCanvas :: MessageReceivedFromTreeSeniorPeer(int32 /*whichDB*/, const String & /*tag*/, const MessageRef & payload)
{
   switch(payload()->what)
   {
      case FRIDGE_REPLY_RANDOMWORD:
      {
         const Point p = payload()->GetPoint("pos");
         UploadNewMagnet(muscleRintf(p.x()), muscleRintf(p.y()), payload()->GetString(FRIDGE_NAME_WORD));
      }
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "FridgeClientCanvas:  Unknown reply Message from server!\n");
         payload()->PrintToStream();
      break;
   }
}

void FridgeClientCanvas :: UploadNewMagnet(int x, int y, const String & word)
{
   MagnetState newMagnet(Point(x, y), word);
   const QSize s = newMagnet.GetScreenRect(fontMetrics()).size();
   newMagnet.SetUpperLeftPos(Point(x-(s.width()/2), y-(s.height()/2)));  // center the new magnet under the mouse pointer
   (void) BeginUndoSequence(String("Create Magnet \"%1\"").Arg(newMagnet.GetText()));

   status_t ret;
   if (UploadMagnetState(GetEmptyString(), &newMagnet, false).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't upload new magnet, error [%s]\n", ret());

   (void) EndUndoSequence();
}

void FridgeClientCanvas :: mouseMoveEvent(QMouseEvent * e)
{
   if (_draggingID.HasChars()) 
   {
      UpdateDraggedMagnetPosition(e->pos(), !_firstMouseMove);
      _firstMouseMove = false;
   }
   e->accept();
}

void FridgeClientCanvas :: mouseReleaseEvent(QMouseEvent * e)
{
   if (_draggingID.HasChars())
   {
      String changeUndoLabel;

      if (rect().contains(e->pos())) UpdateDraggedMagnetPosition(e->pos(), false);
      else
      {
         const MagnetState * ms = _magnets.Get(_draggingID);
         if (ms)
         {
            status_t ret;
            if (UploadMagnetState(_draggingID, NULL, false).IsOK(ret))
            {
               changeUndoLabel = String("Remove Magnet \"%1\"").Arg(ms->GetText());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Couldn't remove deleted magnet, error [%s]\n", ret());
         }
      }

      _draggingID.Clear();

      (void) EndUndoSequence(changeUndoLabel);
   }

   e->accept();
}

void FridgeClientCanvas :: UpdateDraggedMagnetPosition(QPoint mousePos, bool isInterimUpdate)
{
   const MagnetState * ms = _magnets.Get(_draggingID);
   if (ms)
   {
      const QPoint upperLeftPos = (mousePos-_dragDelta);
      MagnetState newState(*ms);
      newState.SetUpperLeftPos(Point(upperLeftPos.x(), upperLeftPos.y()));

      status_t ret;
      if (UploadMagnetState(_draggingID, &newState, isInterimUpdate).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't upload moved magnet, error [%s]\n", ret());
   }
}

status_t FridgeClientCanvas :: UploadMagnetState(const String & optNodeID, const MagnetState * optMagnetState, bool isInterimUpdate)
{
   MessageRef msgRef;

   if (optMagnetState)
   {
      msgRef  = GetMessageFromPool();
      if (msgRef() == NULL) MRETURN_OUT_OF_MEMORY;

      MRETURN_ON_ERROR(optMagnetState->SaveToArchive(*msgRef()));
   }
      
   return UploadTreeNodeValue(optNodeID.Prepend("project/magnets/"), msgRef, isInterimUpdate?TreeGatewayFlags(TREE_GATEWAY_FLAG_INTERIM):TreeGatewayFlags());
}

void FridgeClientCanvas :: TreeGatewayConnectionStateChanged()
{
   ITreeGatewaySubscriber::TreeGatewayConnectionStateChanged();
   if (_magnets.HasItems())
   {
      _magnets.Clear();
      update();
   }

   emit UpdateWindowStatus();
}

void FridgeClientCanvas :: TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & /*optOpTag*/)
{
   if (nodePath.StartsWith("project/magnets/"))
   {
      const bool hadMagnets = HasMagnets();

      const String nodeName = nodePath.Substring("/");
      if (optPayloadMsg())
      {
         MagnetState state;
         if ((state.SetFromArchive(*optPayloadMsg()).IsOK())&&(_magnets.Put(nodeName, state).IsOK())) update();
      }
      else if (_magnets.Remove(nodeName).IsOK()) update();

      if (HasMagnets() != hadMagnets) emit UpdateWindowStatus();  // so the window class can enable or disable the "Clear Magnets" button
   }
   else if (nodePath == "project/last_dragged")
   {
//printf("LAST_DRAGGED IS: %p\n", optPayloadMsg()); if (optPayloadMsg()) optPayloadMsg()->PrintToStream();
      if (optPayloadMsg()) (void) _lastDraggedMagnet.SetFromArchive(*optPayloadMsg());
                      else _lastDraggedMagnet = MagnetState();
      update();
   }
}


void FridgeClientCanvas :: ClearMagnets()
{
   status_t ret;
   (void) BeginUndoSequence("Clear Magnets");
   if (RequestDeleteTreeNodes("project/magnets/*").IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Error requesting deletion of all magnets! [%s]\n", ret());
   (void) EndUndoSequence();
}

}; // end namespace fridge

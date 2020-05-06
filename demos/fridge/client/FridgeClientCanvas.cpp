#include <QPainter>
#include <QMouseEvent>
#include <QWidget>

#include "FridgeClientCanvas.h"
#include "zg/messagetree/gateway/ITreeGateway.h"  // this include is required in order to avoid linker errors(!?)

namespace fridge {

FridgeClientCanvas :: FridgeClientCanvas(ITreeGateway * connector) 
   : ITreeGatewaySubscriber(connector)
{
   (void) AddTreeSubscription("magnets/*");
}

FridgeClientCanvas :: ~FridgeClientCanvas()
{
   // empty
}

void FridgeClientCanvas :: paintEvent(QPaintEvent * e)
{
   QPainter p(this);
   QFontMetrics fm = p.fontMetrics();
   const int th = fm.ascent()+fm.descent();
   const int hMargin = 3;
   const int vMargin = 2;

   p.fillRect(rect(), Qt::lightGray);
   p.setPen(Qt::black);
   p.setBrush(Qt::white);
   for (HashtableIterator<String, MagnetState> iter(_magnets); iter.HasData(); iter++)
   {
      const MagnetState & m = iter.GetValue();
      const Point & pos = m.GetUpperLeftPos();
      QString text = m.GetText()();
#if QT_VERSION >= 0x050B00
      const int tw = fm.horizontalAdvance(text);
#else
      const int tw = fm.width(text);
#endif
      
      const QRect r(pos.x(), pos.y(), tw+(2*hMargin), th+(2*vMargin));
      p.drawRect(r); 
      p.drawText(r, Qt::AlignCenter, text);
   }
}

void FridgeClientCanvas :: mousePressEvent(QMouseEvent * e)
{
   MagnetState newMagnet(Point(e->x(), e->y()), "Foobar");
   MessageRef msgRef = GetMessageFromPool();
   if (newMagnet.SaveToArchive(*msgRef()).IsOK())
   {
      status_t ret;
      if (UploadTreeNodeValue("magnets/", msgRef).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't upload magnet, error [%s]\n", ret());
   }
   e->accept(); 
}

void FridgeClientCanvas :: mouseMoveEvent(QMouseEvent * e)
{
printf("MouseMove!\n");
}

void FridgeClientCanvas :: mouseReleaseEvent(QMouseEvent * e)
{
printf("MouseRelease!\n");
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

}; // end namespace fridge

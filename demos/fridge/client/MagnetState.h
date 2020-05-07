#ifndef MagnetState_h
#define MagnetState_h

#include <QFontMetrics>
#include <QPainter>

#include "common/FridgeNameSpace.h"
#include "support/Point.h"
#include "message/Message.h"
#include "util/String.h"

namespace fridge {

enum {
   MAGNET_TYPE_CODE = 1835100014 // 'magn' 
};

/** An object of this class holds the state of one refrigerator-magnet */
class MagnetState
{
public:
   MagnetState() {/* empty */}
   MagnetState(const Point & upperLeftPos, const String & text) : _upperLeftPos(upperLeftPos) {SetText(text);}

   void SetUpperLeftPos(const Point & p) {_upperLeftPos = p;}
   const Point & GetUpperLeftPos() const {return _upperLeftPos;}

   void SetText(const String & t) {_text = t; _qText = t();}
   const String & GetText() const {return _text;}
   const QString & GetQText() const {return _qText;}

   status_t SaveToArchive(Message & archive) const
   {
      archive.what = MAGNET_TYPE_CODE;
      return archive.CAddPoint("pos", _upperLeftPos) | archive.CAddString("txt", _text);
   }

   status_t SetFromArchive(const Message & archive)
   {
      if (archive.what != MAGNET_TYPE_CODE) return B_BAD_ARGUMENT;
      SetUpperLeftPos(archive.GetPoint("pos"));
      SetText(archive.GetString("txt"));
      return B_NO_ERROR;
   }

   QRect GetScreenRect(const QFontMetrics & fm) const
   {
      const int th = fm.ascent()+fm.descent();
      const int hMargin = 3;
      const int vMargin = 2;

#if QT_VERSION >= 0x050B00
      const int tw = fm.horizontalAdvance(_qText);
#else
      const int tw = fm.width(_qText);
#endif
      return QRect(_upperLeftPos.x(), _upperLeftPos.y(), tw+(2*hMargin), th+(2*vMargin));
   }

   void Draw(QPainter & p, const QRect & r) const
   {
      p.setPen(Qt::black);
      p.setBrush(Qt::white);
      p.drawRect(r);
      p.drawText(r, Qt::AlignCenter, _qText);
   }

private:
   Point _upperLeftPos;
   String _text; 
   QString _qText;   // just I don't want to constantly convert back and forth
};

}; // end namespace fridge

#endif

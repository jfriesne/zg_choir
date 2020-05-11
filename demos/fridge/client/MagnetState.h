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
   /** Default Constructor */
   MagnetState() {/* empty */}

   /** Constructor
     * @param upperLeftPos the (x,y) position of the upper left corner of the magnet (in screen-pixels)
     * @param text the human-readable text that the magnet should display
     */
   MagnetState(const Point & upperLeftPos, const String & text) : _upperLeftPos(upperLeftPos) {SetText(text);}

   /** Set the position of the upper-left corner of the magnet
     * @param p the x/y coordinates, in screen-pixels
     */
   void SetUpperLeftPos(const Point & p) {_upperLeftPos = p;}

   /** Returns the current position of the upper-left corner of the magnet */
   const Point & GetUpperLeftPos() const {return _upperLeftPos;}

   /** Set the human-readable text of the magnet
     * @param t the new text
     */
   void SetText(const String & t) {_text = t; _qText = t();}

   /** Returns the human-readable text of the magnet */
   const String & GetText() const {return _text;}

   /** Returns the human-readable text of the magnet (as a QString, for convenience) */
   const QString & GetQText() const {return _qText;}

   /** Saves the state of this object into a Message
     * @param archive the Message to save the state into
     * @returns B_NO_ERROR on success, or some other error-code on failure.
     */
   status_t SaveToArchive(Message & archive) const
   {
      archive.what = MAGNET_TYPE_CODE;
      return archive.CAddPoint("pos", _upperLeftPos) | archive.CAddString("txt", _text);
   }

   /** Sets the state of this object from a Message
     * @param archive the Message to restore the state from
     * @returns B_NO_ERROR on success, or some other error-code on failure.
     */
   status_t SetFromArchive(const Message & archive)
   {
      if (archive.what != MAGNET_TYPE_CODE) return B_BAD_ARGUMENT;
      SetUpperLeftPos(archive.GetPoint("pos"));
      SetText(archive.GetString("txt"));
      return B_NO_ERROR;
   }

   /** Calculates and returns the on-screen rectangle that this magnet should occupy
     * @param fm the QFontMetrics containing info about the font size we will use
     */
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

   /** Draws this magnet into the specified rectangle 
     * @param p the QPainter to draw with
     * @param r the rectangle to draw into (e.g. as previously returned by GetScreenRect())
     */
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

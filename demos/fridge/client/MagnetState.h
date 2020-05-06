#ifndef MagnetState_h
#define MagnetState_h

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
   MagnetState(const Point & upperLeftPos, const String & text) : _upperLeftPos(upperLeftPos), _text(text) {/* empty */}

   void SetUpperLeftPos(const Point & p) {_upperLeftPos = p;}
   const Point & GetUpperLeftPos() const {return _upperLeftPos;}

   void SetText(const String & t) {_text = t;}
   const String & GetText() const {return _text;}

   status_t SaveToArchive(Message & archive) const
   {
      archive.what = MAGNET_TYPE_CODE;
      return archive.CAddPoint("pos", _upperLeftPos) | archive.CAddString("txt", _text);
   }

   status_t SetFromArchive(const Message & archive)
   {
      if (archive.what != MAGNET_TYPE_CODE) return B_BAD_ARGUMENT;
      _upperLeftPos = archive.GetPoint("pos");
      _text         = archive.GetString("txt");
      return B_NO_ERROR;
   }

private:
   Point _upperLeftPos;
   String _text; 
};

}; // end namespace fridge

#endif

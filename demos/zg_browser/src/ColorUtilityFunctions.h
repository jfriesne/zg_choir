#ifndef ColorUtilityFunctions_h
#define ColorUtilityFunctions_h

#include <QColor>

#include "ZGBrowserNameSpace.h"

namespace zg_browser
{

/** Returns my chosen color for a pretty green selection-bar */
static inline QColor GetZGBrowserSelectColor()
{
   QColor c = Qt::green;
   c.setAlpha(100);
   return c;
}

}  // namespace zg_browser

#endif  // ColorUtilityFunctions_h

#ifndef ZGBrowserNameSpace_h
#define ZGBrowserNameSpace_h

#include "util/String.h"
#include "zg/ZGNameSpace.h"

/** The zg_browser namespace contains the code specific to the ZGBrowser demonstration application.  The zg_browser namespace is a superset of the zg namespace, which is itself a superset of the muscle namespace */
namespace zg_browser
{
   using namespace zg;

/** Convenience macro for getting the UTF8 bytes from a QString easily.
 *  Note that the bytes must be copied out from the returned pointer IMMEDIATELY,
 *  since the returned pointer will become invalid as soon as the implicitly
 *  created string object goes away.
 */
#define FromQ(qs) ((qs).toUtf8().constData())

/** Convert a C string to a QString.
 *  @param cs The UTF8 C string to import.
 *  @returns An equivalent QString.
 */
MUSCLE_NODISCARD inline QString ToQ(const char * cs) {return QString::fromUtf8(cs);}

/** Convenience method: as above, but takes a muscle String object instead of a character pointer. */
MUSCLE_NODISCARD inline QString ToQ(const String & s) {return ToQ(s());}

};  // end namespace zg_browser

#endif  // ZGBrowserNameSpace_h

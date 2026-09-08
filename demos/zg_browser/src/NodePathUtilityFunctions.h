#ifndef NodePathUtilityFunctions_h
#define NodePathUtilityFunctions_h

#include "util/String.h"

#include "ZGBrowserNameSpace.h"

/** Small helpers for living in both the muscle:: and Qt worlds at once.
  * Note that this project never says "using namespace muscle" in a header --
  * muscle and Qt both have String-ish and Message-ish names, so muscle names
  * are always qualified there.  (Inside a .cpp it is fine, and both reference
  * apps do it.)
  */
namespace zg_browser
{

/** Returns the parent path of a session-relative node path ("a/b/c" -> "a/b", "a" -> ""). */
inline String parentPathOf(const String & nodePath)
{
   const int lastSlash = nodePath.LastIndexOf('/');
   return (lastSlash >= 0) ? nodePath.Substring(0, (uint32) lastSlash) : String();
}

/** Returns the final component of a session-relative node path ("a/b/c" -> "c"). */
inline String leafNameOf(const String & nodePath)
{
   const int lastSlash = nodePath.LastIndexOf('/');
   return (lastSlash >= 0) ? nodePath.Substring((uint32) lastSlash + 1) : nodePath;
}

/** Returns the subscription-string that asks for the direct children of (nodePath).
  * The root of the tree is the empty path, whose children are matched by "*".
  */
inline String childrenSubscriptionString(const String & nodePath)
{
   return nodePath.IsEmpty() ? String("*") : (nodePath + "/*");
}

}  // namespace zg_browser

#endif  // NodePathUtilityFunctions_h

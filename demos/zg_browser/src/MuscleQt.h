#pragma once

#include <QString>

#include "util/String.h"

/** Small helpers for living in both the muscle:: and Qt worlds at once.
  * Note that this project never says "using namespace muscle" in a header --
  * muscle and Qt both have String-ish and Message-ish names, so muscle names
  * are always qualified there.  (Inside a .cpp it is fine, and both reference
  * apps do it.)
  */
namespace zgb
{

inline QString toQt(const muscle::String & s)
{
   return QString::fromUtf8(s(), (int) s.Length());
}

inline muscle::String toMuscle(const QString & s)
{
   return {s.toUtf8().constData()};
}

/** Returns the parent path of a session-relative node path ("a/b/c" -> "a/b", "a" -> ""). */
inline muscle::String parentPathOf(const muscle::String & nodePath)
{
   const int lastSlash = nodePath.LastIndexOf('/');
   return (lastSlash >= 0) ? nodePath.Substring(0, (uint32) lastSlash) : muscle::String();
}

/** Returns the final component of a session-relative node path ("a/b/c" -> "c"). */
inline muscle::String leafNameOf(const muscle::String & nodePath)
{
   const int lastSlash = nodePath.LastIndexOf('/');
   return (lastSlash >= 0) ? nodePath.Substring((uint32) lastSlash + 1) : nodePath;
}

/** Returns the subscription-string that asks for the direct children of (nodePath).
  * The root of the tree is the empty path, whose children are matched by "*".
  */
inline muscle::String childrenSubscriptionString(const muscle::String & nodePath)
{
   return nodePath.IsEmpty() ? muscle::String("*") : (nodePath + "/*");
}

}  // namespace zgb

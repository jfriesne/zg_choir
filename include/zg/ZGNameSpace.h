#ifndef ZGNameSpace_h
#define ZGNameSpace_h

#include "support/MuscleSupport.h"

/** The zg namespace contains the public API of the ZG library.
  * The zg namespace is a superset of the muscle namespace
  */
namespace zg
{
   using namespace muscle;
};

/*! \mainpage ZG Documentation Page
 *
 * ZG is a library and API that implements an N-way replicated database and
 * distributed processing system with no central point of failure.  It is designed
 * to be a reference/proof-of-concept implementation to demonstrate the strengths
 * and weaknesses of such a system.
 *
 * ZG is built on top of the MUSCLE networking library, which is included as a
 * captive library within the ZG source code distribution.  More information about
 * MUSCLE can be found at http://www.lcscanada.com/muscle/index.html
 *
 * ZG is distributed with a demonstration application called ZGChoir, which uses
 * ZG to implement a distributed handbell choir -- that is, ZGChoir can be run
 * on one or more computers on the same LAN, and those computers will work together
 * to ring (virtual) bells at the proper times to play a bell-choir song of your choice.
 */

#endif

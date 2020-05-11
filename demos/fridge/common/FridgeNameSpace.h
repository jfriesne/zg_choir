#ifndef FridgeNameSpace_h
#define FridgeNameSpace_h

#include "zg/ZGNameSpace.h"

/** The choir namespace contains the code specific to the Fridge demonstration application.  The fridge namespace is a superset of the zg namespace, which is itself a superset of the muscle namespace */
namespace fridge
{
   using namespace zg;
};

#define FRIDGE_PROGRAM_SIGNATURE "Fridge"  /**< The program signature string used to identify Fridge servers as Fridge servers (and not some other kind of ZGChoir-based server) */

#endif

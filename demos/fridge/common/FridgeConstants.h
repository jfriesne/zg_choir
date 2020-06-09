#ifndef FridgeConstants_h
#define FridgeConstants_h

#include "common/FridgeNameSpace.h"

#define FRIDGE_PROGRAM_SIGNATURE "Fridge"  /**< The program signature string used to identify Fridge servers as Fridge servers (and not some other kind of ZGChoir-based server) */

namespace fridge
{
   enum {
      FRIDGE_COMMAND_GETRANDOMWORD = 1717789539  ///< 'fccc':  FridgeClient sends this to FridgeServer when it wants a new word to place
   };

   enum {
      FRIDGE_REPLY_RANDOMWORD = 1718776434  ///< 'frrr':  FridgeServer sends this reply back to FridgeClient
   };
};

#define FRIDGE_NAME_WORD "word"  ///< FRIDGE_REPLY_RANDOMWORD contains this as a String field

#endif

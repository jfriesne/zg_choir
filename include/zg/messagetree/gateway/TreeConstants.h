#ifndef TreeConstants_h
#define TreeConstants_h

#include "zg/ZGNameSpace.h"

namespace zg {

enum {
   TREE_COMMAND_SETUNDOKEY = 1701147252, ///< 'eert' -- sent from MessageTreeClientConnector to ServerSideMessageTreeSession on TCP connect
};

#define TREE_NAME_UNDOKEY "undokey"  ///< String field containing the undo-key in a TREE_COMMAND_SETUNDOKEY Message

};  // end namespace zg

#endif

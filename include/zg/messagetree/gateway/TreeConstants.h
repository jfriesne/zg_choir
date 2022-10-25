#ifndef TreeConstants_h
#define TreeConstants_h

#include "zg/ZGNameSpace.h"
#include "util/String.h"

namespace zg {

enum {
   TREE_COMMAND_SETUNDOKEY = 1701147252, ///< 'eert' -- sent from MessageTreeClientConnector to ServerSideMessageTreeSession on TCP connect
};

#define TREE_NAME_UNDOKEY "undokey"  ///< String field containing the undo-key in a TREE_COMMAND_SETUNDOKEY Message

// These are parameter-names defined as part of the PR_RESULT_PARAMETERS Message that is downloaded immediately after a client's TCP connection is finalized  */
#define ZG_PARAMETER_NAME_PEERID     "zgpeerid"     /**< String parameter:  peer-ID of the ZGPeer our client is connected to*/
#define ZG_PARAMETER_NAME_SIGNATURE  "zgsignature"  /**< String parameter:  program-signature of the system our client is connected to */
#define ZG_PARAMETER_NAME_SYSTEMNAME "zgsystemname" /**< String parameter:  system-name of the system our client is connected to */
#define ZG_PARAMETER_NAME_ATTRIBUTES "zgattrs"      /**< Message parameter: ZGPeerSettings attributes of the system our client is connected to */
#define ZG_PARAMETER_NAME_NUMDBS     "zgnumdbs"     /**< int8 parameter:    Number of ZG databases in the system our client is connected to */

};  // end namespace zg

#endif

#ifndef ZGConstants_h
#define ZGConstants_h

#include "message/Message.h"
#include "zg/ZGNameSpace.h"

namespace zg
{

#define ZG_VERSION_STRING "1.00"  /**< The current version of the ZG distribution, expressed as an ASCII string */
#define ZG_VERSION        10000   /**< Current version, expressed as decimal Mmmbb, where (M) is the number before the decimal point, (mm) is the number after the decimal point, and (bb) is reserved */

/** Enumeration of port numbers that will be the same for all ZG systems (not currently used) */
enum {
   GLOBAL_PORT_BASE = 41780,   /**< guard value */
};

/** Enumeration of port numbers that will be the same for all peer-processes in a given ZG system */
enum {
   PER_SYSTEM_PORT_BASE = GLOBAL_PORT_BASE+100, /**< guard value */
   PER_SYSTEM_PORT_HEARTBEAT,                   /**< port to transmit heartbeat packets on */
   PER_SYSTEM_PORT_DATA                         /**< port to transmit database packets on  */
};

// Enumeration of port numbers that will be the different for each peer-process in a ZG system (not currently used)
enum {
   PER_PROCESS_PORT_BASE = PER_SYSTEM_PORT_BASE+100  /**< guard value */
};

/** Convenience method:  Given a peer-info Message, returns a single-line string description of that Message (e.g. for debugging)
  * @param peerInfo Reference to a Message containing user-defined attributes describing a particular Peer
  * @returns a human-readable single-line text description of that Message's contents.
  */
String PeerInfoToString(const ConstMessageRef & peerInfo);

};  // end namespace zg

#endif

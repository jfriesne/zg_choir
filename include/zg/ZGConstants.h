#ifndef ZGConstants_h
#define ZGConstants_h

#include "message/Message.h"
#include "zg/ZGNameSpace.h"

namespace zg
{

#define ZG_VERSION_STRING "1.10"  /**< The current version of the ZG distribution, expressed as an ASCII string */
#define ZG_VERSION        11000   /**< Current version, expressed as decimal Mmmbb, where (M) is the number before the decimal point, (mm) is the number after the decimal point, and (bb) is reserved */

#define INVALID_TIME_OFFSET ((int64)(((uint64)-1)/2)) /** Guard value:  Similar to MUSCLE_TIME_NEVER, but for an int64 (relative-offset) time-value rather than an absolute uint64 timestamp */

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

/** Pass-through function for rand_r() (or rand() under Windows, since windows doesn't have rand_r())
  * @param seedp pointer to the seed value to use and update
  */
static inline int GetRandomNumber(unsigned int * seedp)
{
#ifdef WIN32
   (void) seedp;
   return rand();
#else
   return rand_r(seedp);
#endif
}

};  // end namespace zg

#endif

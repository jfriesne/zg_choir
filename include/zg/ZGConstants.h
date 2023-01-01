#ifndef ZGConstants_h
#define ZGConstants_h

#include "message/Message.h"
#include "zg/ZGNameSpace.h"

namespace zg
{

#define ZG_VERSION_STRING "1.20"  /**< The current version of the ZG distribution, expressed as an ASCII string */
#define ZG_VERSION        (12000) /**< Current version, expressed as decimal Mmmbb, where (M) is the number before the decimal point, (mm) is the number after the decimal point, and (bb) is reserved */

#define ZG_COMPATIBILITY_VERSION (0) /**< I'll increment this value whenever ZG's protocol changes in such a way that it breaks compatibility with older versions of ZG */

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

/** Convenience method:  Given a peer-info Message, returns a single-line string description of that Message (eg for debugging)
  * @param peerInfo Reference to a Message containing user-defined attributes describing a particular Peer
  * @returns a human-readable single-line text description of that Message's contents.
  */
String PeerInfoToString(const ConstMessageRef & peerInfo);

/** Convenience method:  Given a compatibility-version code, returns the equivalent human-readable string.
  * @param versionCode a 32-bit compatibility-version code, as returned by CalculateCompatibilityVersionCode() or ZGPeerSettings::GetCompatibilityVersionCode()
  * @returns an equivalent human-reable string, eg "cv0.3"
  */
String CompatibilityVersionCodeToString(uint32 versionCode);

/** Given a ZG compatibility-version and an app-compatibility version, calculates and returns the corresponding 32-bit compatibility-code.
  * @param zgCompatibilityVersion a ZG library compatibility-version number (typically you'd pass ZG_COMPATIBILITY_VERSION here)
  * @param appCompatibilityVersion a application compatilibity-version number (typically you'd pass the value returned by ZGPeerSettings::GetApplicationPeerCompatibilityVersion() here)
  * @returns a 32-bit compatibility-code bit-chord calculated by composing the two argument-values
  */
static inline uint32 CalculateCompatibilityVersionCode(uint16 zgCompatibilityVersion, uint16 appCompatibilityVersion) {return (((uint32)zgCompatibilityVersion)<<16) | ((uint32)appCompatibilityVersion);}

/** Given a 32-bit compatibility-version code, returns the contained ZG compatibility-version number
  * @param code a 32-bit code eg as returned by CalculateCompatibilityVersionCode() or ZGPeerSettings::GetCompatibilityVersionCode()
  */
static inline uint16 GetZGVersionFromCompatibilityVersionCode(uint32 code) {return (code>>16)&0xFFFF;}

/** Given a 32-bit compatibility-version code, returns the contained application-compatibility-version number
  * @param code a 32-bit code eg as returned by CalculateCompatibilityVersionCode() or ZGPeerSettings::GetCompatibilityVersionCode()
  */
static inline uint16 GetAppVersionFromCompatibilityVersionCode(uint32 code) {return code&0xFFFF;}

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

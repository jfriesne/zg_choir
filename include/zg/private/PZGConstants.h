#ifndef PZGConstants_h
#define PZGConstants_h

#include "zg/private/PZGNameSpace.h"
#include "message/Message.h"
#include "util/String.h"

namespace zg_private
{

// Command codes we use internally when sending database-update-requests to the senior peer
enum {
   PZG_PEER_COMMAND_RESET_SENIOR_DATABASE = 2053336420, // 'zcmd'
   PZG_PEER_COMMAND_REPLACE_SENIOR_DATABASE,
   PZG_PEER_COMMAND_UPDATE_SENIOR_DATABASE,
   PZG_PEER_COMMAND_UPDATE_JUNIOR_DATABASE,   // contains a PZGDatabaseUpdate object which will handle all cases
   PZG_PEER_COMMAND_USER_MESSAGE,             // contains an arbitrary user-specified Message
   PZG_PEER_COMMAND_USER_TEXT_MESSAGE,        // e.g. for "all peers echo hi"
};

extern const String PZG_PEER_NAME_USER_MESSAGE;
extern const String PZG_PEER_NAME_DATABASE_ID;
extern const String PZG_PEER_NAME_DATABASE_UPDATE;
extern const String PZG_PEER_NAME_DATABASE_UPDATE_ID;
extern const String PZG_PEER_NAME_TEXT;
extern const String PZG_PEER_NAME_CHECKSUM_MISMATCH;
extern const String PZG_PEER_NAME_BACK_ORDER;

// This is a special/magic database-update-ID value that represents a request for a resend of the entire database
#define DATABASE_UPDATE_ID_FULL_UPDATE ((uint64)-1)

/** Given a PeerInfo Message, tries to return a single-line text description of what's in it (for debugging purposes) */
String PeerInfoToString(const ConstMessageRef & peerInfo);

/** Convenience method:  Given a compatibility-version code, returns the equivalent human-readable string.
  * @param versionCode a 32-bit compatibility-version code, as returned by CalculateCompatibilityVersionCode() or ZGPeerSettings::GetCompatibilityVersionCode()
  * @returns an equivalent human-reable string, e.g. "cv0.3"
  */
String CompatibilityVersionCodeToString(uint32 versionCode);

};  // end namespace zg_private

#endif

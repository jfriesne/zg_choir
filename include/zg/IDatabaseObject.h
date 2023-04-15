#ifndef IDatabaseObject_h
#define IDatabaseObject_h

#include "message/Message.h"
#include "util/RefCount.h"
#include "zg/INetworkTimeProvider.h"
#include "zg/ZGPeerID.h"

namespace zg
{

class ZGDatabasePeerSession;

/** An interface for an object that represents the full contents of a ZG database */
class IDatabaseObject : public RefCountable, public INetworkTimeProvider
{
public:
   /** Default constructor for an IDatabaseObject that is to be used outside the context of a ZGDatabasePeerSession */
   IDatabaseObject() : _session(NULL), _dbIndex(-1) {/* empty */}

   /** Constructor for an IDatabaseObject that is created via ZGDatabasePeerSession::CreateDatabaseObject()
     * @param session pointer to the ZGDatabasePeerSession object that created us, or NULL if we weren't created by a ZGDatabasePeerSession
     * @param dbIndex our position within the ZGDatabasePeerSession's databases-list, or -1 if we weren't created by a ZGDatabasePeerSession
     */
   IDatabaseObject(ZGDatabasePeerSession * session, int32 dbIndex) : _session(session), _dbIndex(dbIndex) {/* empty */}

   /** Destructor */
   virtual ~IDatabaseObject() {/* empty */}

   /** Should be implemented to set this object to its default/just-default-constructed state */
   virtual void SetToDefaultState() = 0;

   /** Should be implemented to replace this object's entire state with the state contained in (archive)
     * @param archive a Message containing state information, that was previously created by calling SaveToArchive()
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t SetFromArchive(const ConstMessageRef & archive) = 0;

   /** Should be implemented to save this object's entire state into (archive).
     * @param archive An empty Message into which to save all of our state information.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t SaveToArchive(const MessageRef & archive) const = 0;

   /** Should return the current checksum of this object.  This checksum should always correspond exactly
     * to this object's current state, and unless this object is quite small, it should be implemented as
     * a running checksum so that this call can just return a known value rather than recalculating the
     * checksum from the data during this call.  That is because this method will be called rather often (eg
     * once after any other call that changes this object's state) and therefore it is better if this call
     * can be made as inexpensive as possible.
     */
   MUSCLE_NODISCARD virtual uint32 GetCurrentChecksum() const = 0;

   /** This method should be implemented to recalculate the database's current checksum from scratch.
     * Note that unlike GetCurrentChecksum(), this method should *not* just returned a precomputed/running
     * checksum, but rather it should grovel over all the data in the database manually.  This method
     * will only be called during debugging sessions (eg to verify that the running checksum is correct)
     * so it is okay if its implementation is relatively expensive.
     */
   MUSCLE_NODISCARD virtual uint32 CalculateChecksum() const = 0;

   /** Should return this object's state as a human-readable string.
     * This method is only used for debugging purposes (eg printing out the state of the database
     * before and after the database is repaired, so the two printouts can be diff'd to see where
     * the error was)
     */
   MUSCLE_NODISCARD virtual String ToString() const = 0;

   /** Returns a pointer to the ZGDatabasePeerSession object that created us, or NULL
     * if this object was not created by a ZGDatabasePeerSession.
     */
   MUSCLE_NODISCARD ZGDatabasePeerSession * GetDatabasePeerSession() const {return _session;}

   /** Returns our index within the ZGDatabasePeerSession object's databses-list, or -1
     * if this object was not created by a ZGDatabasePeerSession.
     */
   MUSCLE_NODISCARD int32 GetDatabaseIndex() const {return _dbIndex;}

protected:
   /** Returns a read-only pointer to the specified IDatabaseObject held by our
     * ZGDatabasePeerSession, or NULL if we don't have a ZGDatabasePeerSession
     * or if the specified database doesn't exist.
     * @param whichDatabase index of the database object we want to access
     */
   MUSCLE_NODISCARD const IDatabaseObject * GetDatabaseObject(uint32 whichDatabase) const;

   /** Returns true iff the peer with the specified ID is currently on line.
     * @param pid The ID of the peer we are interested in.
     * If we have no ZGDatabasePeerSession then this will always return false.
     */
   MUSCLE_NODISCARD bool IsPeerOnline(const ZGPeerID & pid) const;

   /** Returns a table of the currently online peers (and their attributes),
     * or an empty table if we have no ZGDatabasePeerSession.
     */
   MUSCLE_NODISCARD const Hashtable<ZGPeerID, ConstMessageRef> & GetOnlinePeers() const;

   /** Returns the current network time, or 0 if we have no ZGDatabasePeerSession.  */
   MUSCLE_NODISCARD virtual uint64 GetNetworkTime64() const;

   /** Returns the local time corresponding to a given network time, or 0 if we have no ZGDatabasePeerSession.
     * @param networkTime64TimeStamp a network-clock time, in microseconds
     */
   MUSCLE_NODISCARD virtual uint64 GetRunTime64ForNetworkTime64(uint64 networkTime64TimeStamp) const;

   /** Returns the network time corresponding to a given local time, or 0 if we have no ZGDatabasePeerSession.
     * @param runTime64TimeStamp a local-clock time, in microseconds
     */
   MUSCLE_NODISCARD virtual uint64 GetNetworkTime64ForRunTime64(uint64 runTime64TimeStamp) const;

   /** Returns the number of microseconds that should be added to a GetRunTime64() value to turn it into a GetNetworkTime64() value,
     * or subtracted to do the inverse operation.  Note that this value will vary from one moment to the next!
     */
   MUSCLE_NODISCARD virtual int64 GetToNetworkTimeOffset() const;

   /** Returns true iff we are currently executing in a context where it okay to
     * update the local database as a senior-peer (eg we are running in a function that was
     * called by the ZGPeerSession's SeniorUpdateLocalDatabase() method, or similar)
     */
   MUSCLE_NODISCARD bool IsInSeniorDatabaseUpdateContext() const;

   /** Returns true iff we are currently executing in a context where it okay to
     * update the local database as a junior-peer (eg we are running in a function that was
     * called by the ZGPeerSession's JuniorUpdateLocalDatabase() method, or similar)
     * @param optRetSeniorNetworkTime64 if set non-NULL, this value will be set to the network-64 time
     *                                  at which this update was originally handled on the senior peer.
     *                                  If this method returns false, this value will be set to 0.
     */
   MUSCLE_NODISCARD bool IsInJuniorDatabaseUpdateContext(uint64 * optRetSeniorNetworkTime64 = NULL) const;

   /** Should update this object's state using the passed-in senior-do-Message (whose semantics
     * are left up to the subclass to define), and then return a reference to a Message that can
     * be used later to update the junior copies of this database to the same final state that this
     * object is now in.
     * @param seniorDoMsg a Message containing instructions on how to update this object's state.
     * @returns on Success, a reference to a Message that can be used to update the junior peers'
     *          instances of this database to the same state that this object is now in, or
     *          a NULL reference on failure.
     */
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg) = 0;

   /** Should update this object's state using the passed-in junior-do-Messsage (that was previously
     * returned by a call to SeniorUpdate() on the senior peer's instance of this object)
     * @param juniorDoMsg A Message containing instrutions on how to update this object's state.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg) = 0;

   /** Called when our ZGDatabasePeerSession becomes the senior peer, or stops being the senior peer.
     * Call GetDatabasePeerSession()->IsSeniorPeer() to find out which.
     * Default implementation is a no-op.
     */
   virtual void LocalSeniorPeerStatusChanged() {/* empty */}

   /** Called when a new ZGPeer has come online.
     * @param peerID the ID of the newly-online peer.
     * @param optPeerInfo Optional information about the new peer.  May be a NULL reference if no info is available.
     * Default implementation is a no-op.
     */
   virtual void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo) {(void) peerID; (void) optPeerInfo;}

   /** Called when a previously-online ZGPeer has gone offline.
     * @param peerID the ID of the newly-offline peer.
     * @param optPeerInfo information about the departed peer.  May be a NULL reference is no info is available.
     * Default implementation is a no-op.
     */
   virtual void PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo) {(void) peerID; (void) optPeerInfo;}

   /** Returns the current state-ID of our local database */
   MUSCLE_NODISCARD uint64 GetCurrentDatabaseStateID() const;

   /** Returns true iff the local transaction-log currently contains the given transaction ID.
     * @param transactionID to look for in the transaction log.
     */
   MUSCLE_NODISCARD bool UpdateLogContainsUpdate(uint64 transactionID) const;

   /** Given a database transactio-ID, returns the Message-payload of the database-update-transaction with that ID.
     * Note that the returned Message represents the instructions to the junior peers regarding how they should update their local databases.
     * @param transactionID the database-transaction-ID to query about.
     * @returns a valid ConstMessageRef on success, or a NULL ConstMessageRef if no update with the given transaction was found.
     */
   ConstMessageRef GetUpdatePayload(uint64 transactionID) const;

   // Pass-throughs to the ZGDatabasePeerSession object
   virtual status_t RequestResetDatabaseStateToDefault();
   virtual status_t RequestReplaceDatabaseState(const MessageRef & newDatabaseStateMsg);
   virtual status_t RequestUpdateDatabaseState(const MessageRef & databaseUpdateMsg);

protected:
   /** Call this method to send a Message to another MessageTreeDatabaseObject.
     * It will result in MessageReceivedFromMessageTreeDatabaseObject() being called on that object.
     * @param targetPeerID the ZGPeerID of the peer you want the Mesasge to be sent to.  If an invalid/default-constructed
     *                     ZGPeerID is specified, then (msg) will be sent to the specified database-object on all known peers.
     * @param msg the Message you want to send
     * @param optWhichDB If specified, the index of the database-object you want to send to.  If left unspecified/-1, the Message
     *                   will be sent to the database-object with the same index as our own database-index.
     * @returns B_NO_ERROR on success, or another error-code on failure.
     */
   status_t SendMessageToDatabaseObject(const ZGPeerID & targetPeerID, const ConstMessageRef & msg, int32 optWhichDB = -1);

   /** Called when a Message is received from another MessageTreeDatabaseObject.  Default implementation is a no-op.
     * @param msg the Message that was received.
     * @param sourcePeer the ZGPeerID of the peer that sent the Message to us
     * @param sourceDBIdx the database-index of the MessageTreeDatabaseObject that sent us the Message.
     */
   virtual void MessageReceivedFromMessageTreeDatabaseObject(const MessageRef & msg, const ZGPeerID & sourcePeer, uint32 sourceDBIdx) {(void) msg; (void) sourcePeer; (void) sourceDBIdx;}

private:
   friend class ZGDatabasePeerSession;

   ZGDatabasePeerSession * _session;
   int32 _dbIndex;
};
DECLARE_REFTYPES(IDatabaseObject);

};  // end namespace zg

#endif

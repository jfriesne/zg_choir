#ifndef ZGPeerSession_h
#define ZGPeerSession_h

#include "reflector/StorageReflectSession.h"
#include "system/DetectNetworkConfigChangesSession.h"  // for INetworkConfigChangesTarget

#include "zg/INetworkTimeProvider.h"
#include "zg/ZGPeerID.h"
#include "zg/ZGPeerSettings.h"
#include "zg/ZGStdinSession.h"               // for ITextCommandReceiver
#include "zg/ZGConstants.h"                  // for INVALID_TIME_CONSTANT
#include "zg/discovery/server/IDiscoveryServerSessionController.h"

#include "zg/private/PZGBeaconData.h"
#include "zg/private/PZGDatabaseState.h"
#include "zg/private/PZGDatabaseUpdate.h"
#include "zg/private/PZGUpdateBackOrderKey.h"

namespace zg_private
{
   class PZGNetworkIOSession;  // forward declaration
};

namespace zg
{

/** This is the class that a user program would typically subclass from
  * and add to a ReflectServer in order to participate in a ZG system.
  */
class ZGPeerSession : public StorageReflectSession, public ITextCommandReceiver, public INetworkConfigChangesTarget, public INetworkTimeProvider, public IDiscoveryServerSessionController
{
public:
   /** Constructor
     * @param peerSettings the ZGPeerSettings that this system is to use.
     */
   ZGPeerSession(const ZGPeerSettings & peerSettings);

   /** Called when this session object is attached to the ReflectServer object.
     * @returns B_NO_ERROR iff the attaching process went okay, or an error value if there was an error and the attachment should be aborted.
     */
   virtual status_t AttachedToServer();

   /** Called just before this session object is detached from the ReflectServer object. */
   virtual void AboutToDetachFromServer();

   /** Call this to request the termination of this session and its detachment from the ReflectServer.
     * Overridden to do some thread-shutdown work to avoid race conditions.
     */
   virtual void EndSession();

   /** Returns the time (in microseconds, using the timebase of GetRunTime64()) when Pulse() should next be called.
     * @param args Context information for this call (including the current time and the time at which Pulse() was supposed to be called at)
     * @returns a new time in microseconds, or MUSCLE_TIME_NEVER if we don't need Pulse() to be called.
     */
   virtual uint64 GetPulseTime(const PulseArgs & args);

   /** Called at (approximately) the time specified by GetPulseTime().
     * @param args Context information for this call (including the current time and the time at which this method was supposed to be called at)
     */
   virtual void Pulse(const PulseArgs & args);

   /** Default implementation returns true iff we are currently fully attached to the ZG system. */
   virtual bool IsReadyForTextCommands() const {return _iAmFullyAttached;}

   /** Default implementation handles some standard ZG commands such as "print peers" or "print sessions".
     * @param text A text string was received via our stdin stream.
     * @returns true if the command was recognized and handled, or false otherwise.
     */
   virtual bool TextCommandReceived(const String & text);

   /** Returns true iff this peer is currently considered to be the senior peer of the system. */
   bool IAmTheSeniorPeer() const;

   /** Returns the ZGPeerSettings object, as passed to our constructor. */
   const ZGPeerSettings & GetPeerSettings() const {return _peerSettings;}

   /** Called when the set of available network interfaces on this computer has changed.
     * Default implementation is a no-op.
     * @param optInterfaceNames An optional list of network-interface names (e.g. "eth0", "eth1") that are known to have
     *                          changed.  If empty, then you should assume that any or all network interfaces have changed.
     */
   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames) {(void) optInterfaceNames;}

   /** Called just before this computer goes to sleep (e.g. due to closing a laptop lid) */
   virtual void ComputerIsAboutToSleep() {_iAmFullyAttached = false;}  // we'll re-fully-attach after we wake up again

   /** Called just after this computer woke up from sleep (e.g. due to opening a laptop lid) */
   virtual void ComputerJustWokeUp() {/* empty */}

   /** Returns true iff this peer is fully attached to the system (i.e. has completed enumeration of which other peers are online, etc) */
   bool IAmFullyAttached() const {return _iAmFullyAttached;}

   /** Called whenever this peer has gained or lost senior-peer status (current status is indicated by the return value of IAmTheSeniorPeer()) */
   virtual void LocalSeniorPeerStatusChanged();

   /** Returns the ZGPeerID being used by this local peer.  Returned value is not valid until after ZGPeerSession::AttachedToServer() returns B_NO_ERROR. */
   const ZGPeerID & GetLocalPeerID() const {return _localPeerID;}

   /** Returns the ZGPeerID of the senior peer of this system, or an invalid ZGPeerID if there currently is no senior peer (that we know of). */
   const ZGPeerID & GetSeniorPeerID() const {return _seniorPeerID;}

   /** Returns the current time according to the network-time-clock, in microseconds.
     * The intent of this clock is to be the same on all peers in the system.  However, this means that it may occasionally
     * change (break monotonicity) in order to synchronize with the other peers in the system.
     * Note that this function will return 0 if we aren't currently fully attached to the system, since before then we don't know the network time.
     */
   virtual uint64 GetNetworkTime64() const {return _iAmFullyAttached ? GetNetworkTime64ForRunTime64(GetRunTime64()) : 0;}

   /** Given a network-time-clock-value (i.e. one using the same time-base as returned by GetNetworkTime64()), 
     * returns the approximately-equivalent local-time-clock-value (i.e. one using the same time-base as returned by GetRunTime64())  
     */
   virtual uint64 GetRunTime64ForNetworkTime64(uint64 networkTime64TimeStamp) const
   {
      const int64 ntto = GetToNetworkTimeOffset();
      return ((ntto==INVALID_TIME_OFFSET)||(networkTime64TimeStamp==MUSCLE_TIME_NEVER))?MUSCLE_TIME_NEVER:(networkTime64TimeStamp-ntto);
   }

   /** Given a local-time-clock-value (i.e. one using the same time-base as returned by GetRunTime64()), returns 
     * the approximately equivalent network-time-value (i.e. one using the same time-base as returned by GetNetworkTime64())
     */
   virtual uint64 GetNetworkTime64ForRunTime64(uint64 runTime64TimeStamp) const 
   {
      const int64 ntto = GetToNetworkTimeOffset();
      return ((ntto==INVALID_TIME_OFFSET)||(runTime64TimeStamp==MUSCLE_TIME_NEVER))?MUSCLE_TIME_NEVER:(runTime64TimeStamp+ntto);
   }

   /** Returns the number of microseconds that should be added to a GetRunTime64() value to turn it into a GetNetworkTime64() value,
     * or subtracted to do the inverse operation.  Note that this value will vary from one moment to the next!
     */
   virtual int64 GetToNetworkTimeOffset() const;

   /** Returns a reference to a read-only table of the peers that are currently online in the system.
     * The keys in the table are the peer's IDs, and the values are the peers' attributes (may be NULL if they didn't advertise any)
     * The ordering of the entries in the table is not significant (in particular, it doesn't reflect the ordering of the peers' seniority, at least not for now).
     */
   const Hashtable<ZGPeerID, ConstMessageRef> & GetOnlinePeers() const {return _onlinePeers;}

   /** Returns true iff if the specified peer is currently online.
     * @param peerID The peer ID to check on.
     */
   bool IsPeerOnline(const ZGPeerID & peerID) const;

   /** Returns true iff we are currently executing in a context where it okay to
     * update the specified local database as a senior-peer (e.g. we are running in a function that was
     * called by the SeniorUpdateLocalDatabase() method, or similar)
     * @param whichDB index of the database we are inquiring about
     */
   bool IsInSeniorDatabaseUpdateContext(uint32 whichDB) const;

   /** Returns true iff we are currently executing in a context where it okay to
     * update the local database as a junior-peer (e.g. we are running in a function that was
     * called by the JuniorUpdateLocalDatabase() method, or similar)
     * @param whichDB index of the database we are inquiring about
     */
   bool IsInJuniorDatabaseUpdateContext(uint32 whichDB) const;

   /** Gets our current estimate of the one-way network latency between us and the specified peer.
     * @param peerID The peer ID to get the latency of
     * @returns The estimated latency, in microseconds, or MUSCLE_TIME_NEVER if the latency is unknown.
     */
   uint64 GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const;

protected:
   /** Call this if you want to request that the specified database be reset back to its well-known default state.
     * The well-known default state is defined by the implementation of the subclass's ResetLocalDatabaseToDefault() method.
     * Note that this method only sends the request; the actual reset will happen (if it happens) some time after this method returns.
     * @param whichDatabase the index of the database that should be reset
     * @returns B_NO_ERROR if the the reset-request was successfully sent to the senior peer, or an error code if the request could not be sent.
     */
   status_t RequestResetDatabaseStateToDefault(uint32 whichDatabase);

   /** Call this if you want to request that the specified database be completely replaced with a new state that is specified by the passed-in Message.
     * The new database state will be created by calling SetLocalDatabaseFromMessage(newDatabaseStateMsg) on the senior peer.
     * Note that this method only sends the request; the actual database-replace will happen (if it happens) some time after this method returns.
     * @param whichDatabase the index of the database whose state should be replaced.
     * @param newDatabaseStateMsg a Message containing instructions/data that SetLocalDatabaseFromMessage() can use later on to create a new database state.
     * @returns B_NO_ERROR if the the replace-request was successfully sent to the senior peer, or an error code if the request could not be sent.
     */
   status_t RequestReplaceDatabaseState(uint32 whichDatabase, const MessageRef & newDatabaseStateMsg);

   /** Call this if you want to request that the specified database be incrementally updated to a new state via a specified passed-in Message.
     * The new database state will be created by calling SeniorUpdateLocalDatabase(databaseUpdateMsg) on the senior peer, and then the
     * Message returned by that method call will be used to update the local database of all of the other peers.
     * Note that this method only sends the request; the actual database-update will happen (if it happens) some time after this method returns.
     * @param whichDatabase the index of the database whose state should be updated.
     * @param databaseUpdateMsg a Message containing instructions/data that SeniorUpdateLocalDatabase() can use later on to transition the database to a new database state.
     * @returns B_NO_ERROR if the the update-request was successfully sent to the senior peer, or an error code if the request could not be sent.
     */
   status_t RequestUpdateDatabaseState(uint32 whichDatabase, const MessageRef & databaseUpdateMsg);

   /** This method will be called when a message is sent to us by another peer.
     * A subclass may override this method to catch any user-defined Messages that other
     * peers might want to send it.  The default implementation of this method just prints
     * an "Unknown Message Reeceived" warning to the log.
     * @param fromPeerID The unique ID of the peer who sent the Message to this peer.
     * @param msg The Message that was sent to this peer.
     */
   virtual void MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg);

   /** Must be implemented to reset local state of the specified database to its well-known default state.
     * @param whichDatabase The index of the database to reset (e.g. 0 for the first database, 1 for the second, and so on)
     * @param dbChecksum Passed in as the database's current checksum value.  On return, this should be set to the database's new checksum value.
     */
   virtual void ResetLocalDatabaseToDefault(uint32 whichDatabase, uint32 & dbChecksum) = 0;

   /** This method will only be called on the senior peer.  It must be implemented to update the senior peer's local database
     * and return a MessageRef that the system can later use to update same database on the various junior peers in the same way later on.
     * @param whichDatabase The index of the database to update (e.g. 0 for the first database, 1 for the second, and so on)
     * @param dbChecksum Passed in as the database's current checksum value.  On return, this should be set to the database's new (post-update) checksum value.
     * @param seniorDoMsg Reference to a Message instructing the senior peer how his local database should be updated.  (The contents and semantics of this Message
     *                    will be determined by logic in the subclass of this class; they are not specified by the ZGPeerSession class itself)
     * @returns on success, this method should return a MessageRef that can be sent on to the junior peers to tell them how to update their
     *          local database so that their local database ends up in the same post-change state that our local database is currently in.
     *          Returning (seniorDoMsg) is acceptable if that is a Message that will cause the junior peers to do the right thing.
     *          On failure (or refusal-to-handle-the-update), a NULL MessageRef() should be returned.
     */
   virtual ConstMessageRef SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg) = 0;

   /** This method will only be called on junior peers.  It must be implemented to update the junior peer's local database
     * according to the instructions contained in (juniorDoMsg).
     * @param whichDatabase The index of the database to update (e.g. 0 for the first database, 1 for the second, and so on)
     * @param dbChecksum Passed in as the database's current checksum value.  On return, this should be set to the database's new (post-update) checksum value.
     * @param juniorDoMsg Reference to a Message instructing the junior peer how his lcoal database should be updated. (The contents and semantics of this Message
     *                    will be determined by logic in the subclass of this class; they are not specified by the ZGPeerSession class itself)
     * @returns on success, returns B_NO_ERROR.  On failure (or refusal-to-update), returns an error code.
     */
   virtual status_t JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg) = 0;

   /** This method should be implemented to save the state of the specified local database into a Message.
     * @param whichDatabase The index of the database to save (e.g. 0 for the first database, 1 for the second, and so on)
     * @returns a MessageRef containing the full current state of the local database, or a NULL MessageRef on failure.
     */
   virtual MessageRef SaveLocalDatabaseToMessage(uint32 whichDatabase) const = 0;

   /** This method should be implemented to replace the current state of the specified local database with the
     * new state represented by the passed-in Message.
     * @param whichDatabase The index of the database to replace (e.g. 0 for the first database, 1 for the second, and so on)
     * @param dbChecksum Passed in as the database's current checksum value.  On return, this should be set to the database's new (post-update) checksum value.
     * @param newDBStateMsg A Message holding the contents of the new database we want to replace the current database with.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t SetLocalDatabaseFromMessage(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & newDBStateMsg) = 0;

   /** This method is used for sanity-checking.  It should be implemented to scan the specified local database
     * and return a checksum representing all of the data in the database.  This checksum should match the
     * checksums returned/updated by the SeniorUpdateLocalDatabase() and JuniorUpdateLocalDatabase() functions for
     * the same database state.
     */
   virtual uint32 CalculateLocalDatabaseChecksum(uint32 whichDatabase) const = 0;

   /** This method may be implemented to return a human-readable representation of the specified database's current contents
     * as a String.  This string will be printed to stdout after a checksum error has occurred, to make it easier to debug
     * the update logic (i.e. by comparing the database state before and after the checksum-mismatched database was replaced,
     * to see what is different).
     * The default implementation of this method just returns "(GetLocalDatabaseContentsAsString unimplemented)".
     * @param whichDatabase The index of the database to return a string for.
     */
   virtual String GetLocalDatabaseContentsAsString(uint32 whichDatabase) const;

   /** Called when a new peer has joined our group.
     * @param peerID The unique ID of the peer who is now online.  (Note that this ID may be our own ID!)
     * @param optPeerInfo An optional Message containing some additional information about that peer, if that peer specified
     *                    any additional information in his ZGPeerSettings object.  May be a NULL reference otherwise.
     */
   virtual void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);

   /** Called when a peer has left our group.
     * @param peerID The unique ID of the peer who is now offline.
     * @param optPeerInfo An optional Message containing some additional information about that peer, if that peer specified
     *                    any additional information in his ZGPeerSettings object.  May be a NULL reference otherwise.
     */
   virtual void PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);

   /** Called when the senior peer of our peer group has changed.
     * @param oldSeniorPeerID The unique ID of the peer who was the senior peer but no longer is.  (May be a zero/invalid if there wasn't any senior peer before)
     * @param newSeniorPeerID The unique ID of the peer who is now the senior peer.  (May be a zero/invalid if there is no senior peer anymore)
     */
   virtual void SeniorPeerChanged(const ZGPeerID & oldSeniorPeerID, const ZGPeerID & newSeniorPeerID);

   /** Tries to send the given Message to all peers (except this one) via UDP multicast.
     * The PacketTunnelIOGateway mechanism is used so that large Messages can
     * be transmitted as well as small ones.  Delivery is not guaranteed, however.
     * @param msg The Message object to send.  MessageReceivedFromPeer() will be called on each peer when the Message arrives there.
     * @returns B_NO_ERROR if the Message was successfully enqueued to be multicasted out, of an error code otherwise.
     */
   status_t SendMulticastUserMessageToAllPeers(const MessageRef & msg);

   /** Tries to send the given Message to all peers via multiple instances of TCP unicast.
     * @param msg The Message object to send.  MessageReceivedFromPeer() will be called on each peer when the Message arrives there.
     * @param sendToSelf Whether the message should be send to the sending peer (this) (defaults to true).
     * @returns B_NO_ERROR if the Message was successfully enqueued to be multicasted out, of an error code otherwise.
     */
   status_t SendUnicastUserMessageToAllPeers(const MessageRef & msg, bool sendToSelf = true);

   /** Tries to send the given Message to a specific peers via TCP unicast.
     * @param destinationPeerID The ZGPeerID of the peer we want this Message to be sent to.
     * @param msg The Message object to send.  MessageReceivedFromPeer() will be called on the peer when the Message arrives there.
     * @returns B_NO_ERROR if the Message was successfully enqueued to be unicasted out, of an error code otherwise.
     */
   status_t SendUnicastUserMessageToPeer(const ZGPeerID & destinationPeerID, const MessageRef & msg);

   /** Prints various database-state information to stdout.  Useful for debugging purposes.
     * @param whichDatabase Index of the database to print out state for, or leave set to -1 to print out info about all databases.
     */
   void PrintDatabaseStateInfo(int32 whichDatabase = -1) const;

   /** Prints the database update-log(s) to stdout.  Useful for debugging purposes.
     * @param whichDatabase Index of the database to print out the update-log for, or leave set to -1 to print out the update-logs of all databases.
     */
   void PrintDatabaseUpdateLog(int32 whichDatabase = -1) const;

   /** From the IDiscoveryServerSessionController API:  Given an incoming discovery-ping, returns a
     * useful output discovery-pong to go back to the client.
     * @param pingMsg containing the incoming ping
     * @param pingSource the IP address and port that the ping packet came from
     * @returns How many microseconds to delay before sinding the pong back out, or MUSCLE_TIME_NEVER to not send any pong back out.
     * @note on return, (pingMsg) will be updated to point to the appropriate pong-Message instead.
     */
   virtual uint64 HandleDiscoveryPing(MessageRef & pingMsg, const IPAddressAndPort & pingSource);

   /** Returns the current state-ID of the locally held database.
     * @param whichDB index of the database to return the state-ID of
     * @note returns 0 if (whichDB) isn't a valid database index.
     */
   uint64 GetCurrentDatabaseStateID(uint32 whichDB) const;

   /** Returns true iff the given database's local transaction-log currently contains the given transaction ID.
     * @param whichDB index of the database to return the oldest accessible state-ID of.
     * @param transactionID to look for in the transaction log.
     */
   bool UpdateLogContainsUpdate(uint32 whichDB, uint64 transactionID) const;

   /** Returns a read-only reference to the transaction-payload for the given transaction ID in the given database.
     * Note that this Message contains the instructions to junior peers for how they should update their local database;
     * it does not contain the instructions sent to the senior peer for creating the update.
     * @param whichDB index of the database to return a transactio-payload from
     * @param transactionID the transaction ID to look up
     * @returns a valid ConstMessageRef on success, or a NULL ConstMessageRef if the given payload could not be found.
     */
   ConstMessageRef GetUpdatePayload(uint32 whichDB, uint64 transactionID) const;

   /** Returns a list of unicast IPAddressAndPort locations we have on file for the specified ZGPeer.
     * @param peerID The unique ID of peer in question.
     */
   Queue<IPAddressAndPort> GetUnicastIPAddressAndPortsForPeerID(const ZGPeerID & peerID) const;

private:
   void ScheduleSetBeaconData();
   void ShutdownChildSessions();
   status_t SendRequestToSeniorPeer(uint32 whichDatabase, uint32 whatCode, const MessageRef & userMsg);
   status_t HandleDatabaseUpdateRequest(const ZGPeerID & fromPeerID, const MessageRef & msg, bool isMessageMeantForSeniorPeer);
   status_t SendDatabaseUpdateViaMulticast(const zg_private::ConstPZGDatabaseUpdateRef  & dbUp);
   status_t RequestBackOrderFromSeniorPeer(const zg_private::PZGUpdateBackOrderKey & ubok);
   zg_private::ConstPZGBeaconDataRef GetNewSeniorBeaconData() const;
   status_t SendUnicastInternalMessageToAllPeers(const MessageRef & msg, bool sendToSelf = true);
   status_t SendUnicastInternalMessageToPeer(const ZGPeerID & destinationPeerID, const MessageRef & msg);
   status_t SendMulticastInternalMessageToAllPeers(const MessageRef & internalMsg);

   // These methods are called from the PZGNetworkIOSession code
   void PrivateMessageReceivedFromPeer(const ZGPeerID & peerID, const MessageRef & msg);
   void BeaconDataChanged(const zg_private::ConstPZGBeaconDataRef & beaconData);
   void BackOrderResultReceived(const zg_private::PZGUpdateBackOrderKey & ubok, const zg_private::ConstPZGDatabaseUpdateRef & optUpdateData);
   zg_private::ConstPZGDatabaseUpdateRef GetDatabaseUpdateByID(uint32 whichDatabase, uint64 updateID) const;

   const ZGPeerSettings _peerSettings;

   const ZGPeerID _localPeerID;
   ZGPeerID _seniorPeerID;

   AbstractReflectSessionRef _networkIOSession;
   bool _iAmFullyAttached;

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
   friend class zg_private::PZGDatabaseState;
   friend class zg_private::PZGNetworkIOSession;
#endif

   Queue<zg_private::PZGDatabaseState> _databases;
   bool _setBeaconDataPending;

   Hashtable<ZGPeerID, ConstMessageRef> _onlinePeers;
};
DECLARE_REFTYPES(ZGPeerSession);

};  // end namespace zg

#endif

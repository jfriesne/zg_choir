#ifndef PZGNetworkIOSession_h
#define PZGNetworkIOSession_h

#include "system/DetectNetworkConfigChangesSession.h"
#include "zg/ZGPeerID.h"
#include "zg/ZGPeerSession.h"
#include "zg/ZGPeerSettings.h"
#include "zg/private/PZGBeaconData.h"
#include "zg/private/PZGDatabaseUpdate.h"
#include "zg/private/PZGThreadedSession.h"
#include "zg/private/PZGUnicastSession.h"
#include "zg/private/PZGHeartbeatSession.h"
#include "zg/private/PZGHeartbeatSettings.h"
#include "zg/private/PZGUpdateBackOrderKey.h"

namespace zg_private
{

/** This session manages the ZG process's data I/O over the network.  It handles
  * both multicast and unicast traffic, incoming and outgoing, as necessary.
  */
class PZGNetworkIOSession : public PZGThreadedSession, public INetworkConfigChangesTarget
{
public:
   PZGNetworkIOSession(const ZGPeerSettings & peerSettings, const ZGPeerID & localPeerID, ZGPeerSession * master);

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void EndSession();

   /** Enqueues the given Message object to be sent to the specified peer via unicast/TCP.
     * @param peerID The peer ID to send the Message to.
     * @param msg The Message to send to the specified peer.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SendUnicastMessageToPeer(const ZGPeerID & peerID, const ConstMessageRef & msg);

   /** Sends the specified Message to all peers via multicast/UDP/PacketTunnel.
     * @param msg Should be one of the PZG_PEER_COMMAND_* Message types.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SendMulticastMessageToAllPeers(const ConstMessageRef & msg) {return SendMessageToInternalThread(CastAwayConstFromRef(msg));}

   /** Sends the specified Message to all peers via unicast/TCP.
     * @param msg Should be one of the PZG_PEER_COMMAND_* Message types.
     * @param sendToSelf Whether the message should be send to the sending peer (this) (defaults to true).
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SendUnicastMessageToAllPeers(const ConstMessageRef & msg, bool sendToSelf = true);

   /** Tells the multicast I/O thread what beacon data to transmit periodically (if any)
     * @param optBeaconData If non-NULL, this is the data to transmit every so often.  If NULL, no beacon data should be transmitted.
     */
   status_t SetBeaconData(const ConstPZGBeaconDataRef & optBeaconData);

   /** Request that the senior peer send us the specified database update via unicast. */
   status_t RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok, bool dueToChecksumError);

   /** Returns true iff the specified peer is currently online */
   MUSCLE_NODISCARD bool IsPeerOnline(const ZGPeerID & id) const {return GetMainThreadPeers().ContainsKey(id);}

   /** Returns the current estimated one-way network latency to the specified peer, in microseconds */
   MUSCLE_NODISCARD uint64 GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const;

   MUSCLE_NODISCARD const ConstPZGHeartbeatSettingsRef & GetHeartbeatSettings() const {return _hbSettings;}

   /** Returns the UDP port number where our heartbeat thread is accepting incoming time-sync UDP packets from clients, or 0 if it isn't currently accepting them. */
   MUSCLE_NODISCARD uint16 GetTimeSyncUDPPort() const;

   MUSCLE_NODISCARD virtual const char * GetTypeName() const {return "Network I/O Master";}

   // PulseNode interface
   MUSCLE_NODISCARD virtual uint64 GetPulseTime(const PulseArgs &);
   virtual void Pulse(const PulseArgs & args);

   // INetworkConfigChangesTarget interface
   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames);
   virtual void ComputerIsAboutToSleep();
   virtual void ComputerJustWokeUp();

   MUSCLE_NODISCARD const ConstPZGHeartbeatSettingsRef & GetPZGHeartbeatSettings() const {return _hbSettings;}

   MUSCLE_NODISCARD const Hashtable<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > & GetMainThreadPeers() const
   {
      return _hbSession() ? _hbSession()->GetMainThreadPeers() : GetDefaultObjectForType< Hashtable<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > >();
   }

   MUSCLE_NODISCARD const ZGPeerID & GetLocalPeerID() const {return _localPeerID;}

   ConstPZGDatabaseUpdateRef GetDatabaseUpdateByID(uint32 whichDB, uint64 updateID) const;
   void VerifyOrFixLocalDatabaseChecksum(uint32 whichDB);

   MUSCLE_NODISCARD int64 GetToNetworkTimeOffset() const;

   MUSCLE_NODISCARD IPAddressAndPort GetUnicastIPAddressAndPortForPeerID(const ZGPeerID & peerID, uint32 sourceIndex=0) const;

   MUSCLE_NODISCARD const INetworkInterfaceFilter * GetNetworkInterfaceFilter() const;

protected:
   virtual void InternalThreadEntry();
   virtual void MessageReceivedFromInternalThread(const MessageRef & msg, uint32 numLeft);

private:
   friend class PZGUnicastSession;
   friend class PZGHeartbeatSession;

   // These methods are called by the PZGHeartbeatSession
   void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);
   void PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);
   void SeniorPeerChanged(const ZGPeerID & oldSeniorPeerID, const ZGPeerID & newSeniorPeerID);

   PZGUnicastSessionRef GetUnicastSessionForPeerID(const ZGPeerID & peerID, bool allocIfNecessary);

   void RegisterUnicastSession(PZGUnicastSession * session);
   void UnregisterUnicastSession(PZGUnicastSession * session);
   void ClearAllUnicastSessions();
   void ClearHeartbeatSession();
   void UnicastMessageReceivedFromPeer(const ZGPeerID & remotePeerID, const MessageRef & msg);
   void ShutdownChildSessions();
   MUSCLE_NODISCARD bool IAmTheSeniorPeer() const {return _seniorPeerID == _localPeerID;}
   void BackOrderResultReceived(const PZGUpdateBackOrderKey & ubok, const ConstPZGDatabaseUpdateRef & optUpdateData);
   status_t SetupHeartbeatSession();

   const ZGPeerSettings _peerSettings;
   const ZGPeerID _localPeerID;
   const uint64 _beaconIntervalMicros;
   ConstPZGHeartbeatSettingsRef _hbSettings;

   // all the stuff below should be accessed from the main thread only
   ZGPeerSession * _master;
   DetectNetworkConfigChangesSessionRef _dnccSession;  // notifies us when network interfaces have come online or gone offline
   PZGHeartbeatSessionRef _hbSession;       // handles sending/receiving of heartbeat packets and updates the ordered-online-peers-list for us
   Hashtable<ZGPeerID, Queue<PZGUnicastSessionRef> > _namedUnicastSessions;  // unicast sessions whose remote endpoint we do know
   Hashtable<PZGUnicastSessionRef, Void> _registeredUnicastSessions;         // all unicast sessions (whether we know their endpoint or not)
   Queue<ConstMessageRef> _messagesSentToSelf;  // just because I think it's silly to serialize and then deserialize a MessageRef to myself
   ZGPeerID _seniorPeerID;
   bool _computerIsAsleep;

   Mutex _hbSessionPtrMutex;
   PZGHeartbeatSession * _hbSessionPtr; // this separate pointer is maintained just so the main thread can access it without provoking the ThreadSanitizer
};
DECLARE_REFTYPES(PZGNetworkIOSession);

};  // end namespace zg_private

#endif

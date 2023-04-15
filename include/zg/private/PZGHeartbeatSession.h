#ifndef PZGHeartbeatSession_h
#define PZGHeartbeatSession_h

#include "zg/ZGPeerID.h"
#include "zg/private/PZGThreadedSession.h"
#include "zg/private/PZGHeartbeatPacket.h"
#include "zg/private/PZGHeartbeatSettings.h"
#include "zg/private/PZGHeartbeatThreadState.h"

namespace zg_private
{

class PZGNetworkIOSession;

/** This session manages the ZG process's heartbeat thread.  The heartbeat thread
  * is responsible for notifying all other processes in our ZG system about our presence
  * and status, and also for keeping us notified about their presence and status.
  *
  */
class PZGHeartbeatSession : public PZGThreadedSession
{
public:
   PZGHeartbeatSession(const ConstPZGHeartbeatSettingsRef & settings, PZGNetworkIOSession * master);

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void EndSession();

   MUSCLE_NODISCARD virtual const char * GetTypeName() const {return "Heartbeat";}

   MUSCLE_NODISCARD const Hashtable<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > & GetMainThreadPeers() const {return _mainThreadPeers;}

   status_t SendMessageToHeartbeatThread(const MessageRef & msg) {return SendMessageToInternalThread(msg);}

   MUSCLE_NODISCARD int64 MainThreadGetToNetworkTimeOffset() const {return _hbtState.MainThreadGetToNetworkTimeOffset();}
   MUSCLE_NODISCARD uint16 MainThreadGetTimeSyncUDPPort()    const {return _timeSyncUDPPort;}

   /** Returns the current estimated one-way network latency to the specified peer, in microseconds */
   MUSCLE_NODISCARD uint64 GetEstimatedLatencyToPeer(const ZGPeerID & peerID) const;

protected:
   virtual void InternalThreadEntry();
   virtual void MessageReceivedFromInternalThread(const MessageRef & msg, uint32 numLeft);

private:
   MUSCLE_NODISCARD const ZGPeerID & GetSeniorPeerID() const;

   ConstPZGHeartbeatSettingsRef _hbSettings;
   PZGNetworkIOSession * _master;

   Hashtable<ZGPeerID, Queue<ConstPZGHeartbeatPacketWithMetaDataRef> > _mainThreadPeers;  // Queue because each peer may be coming in from multiple sources

   PZGHeartbeatThreadState _hbtState;

   ConstSocketRef _timeSyncUDPSocket;  // allocated in main thread, used by heartbeat-thread
   uint16 _timeSyncUDPPort;            // UDP port that _timeSyncUDPSocket is listening for incoming traffic on
};
DECLARE_REFTYPES(PZGHeartbeatSession);

};  // end namespace zg_private

#endif

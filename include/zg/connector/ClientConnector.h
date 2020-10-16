#ifndef ClientConnector_h
#define ClientConnector_h

#include <atomic>
#include "message/Message.h"
#include "regex/QueryFilter.h"
#include "zg/callback/ICallbackSubscriber.h"
#include "zg/clocksync/ZGTimeAverager.h"
#include "zg/gateway/INetworkMessageSender.h"
#include "zg/INetworkTimeProvider.h"
#include "zg/ZGConstants.h"  // for INVALID_TIME_OFFSET

namespace zg {

class ClientConnectorImplementation;

/** Abstract front-end to ZG's TCP-client-connector logic.
  * It handles setting up, maintaining, and (if necessary) reconnecting a TCP connection to one of the ZG system's servers. 
  * Any required functionality while the TCP connection is active is delegated to a concrete subclass.
  */
class ClientConnector : public ICallbackSubscriber, public INetworkMessageSender, public INetworkTimeProvider, public RefCountable
{
public:
   /** Constructor.
     * @param mechanism pointer to the CallbackMechanism our I/O thread can use to call callback-methods in the context of the main thread
     * @note be sure to call Start() to start the network I/O thread running, otherwise this object won't do anything useful.
     */
   ClientConnector(ICallbackMechanism * mechanism);

   /** Destructor */
   ~ClientConnector();

   /** Starts the network I/O thread, and tells it what sort of server it should be trying to connect to.
     * @param signaturePattern signature string of the type(s) of ZG program you want to connect to.
     *                         May be wildcarded if you aren't particular about the type of server you want to connect to.
     *                         (e.g. passing "*" will allow you to connect to any kind of ZG server)
     * @param systemNamePattern Name of the ZG system we want to connect to.  May be wildcarded if you aren't particular about
     *                          the name of the system you connect to.  (e.g. passing "*" will get you connected to an available
     *                          system regardless of what its system-name is)
     * @param optAdditionalDiscoveryCriteria optional reference to a QueryFilter that describes any additional criteria regarding
     *                          what sorts of server, in particular, you are interested in.  Servers whose
     *                          discovery-reply-Messages don't match this QueryFilter's criteria will not be connected to.
     *                          May be NULL if you only care about the server's signature and system name.
     * @param autoReconnectTimeMicroseconds how long to wait before trying to auto-reconnect, when our TCP connection gets severed.  Defaults to 250 milliseconds.
     * @returns B_NO_ERROR on success, or an error code if setup failed.
     * @note If called with with the same arguments that are already in use, this method will just return B_NO_ERROR without doing anything else.
     *       Otherwise, this method will call Stop() and then set up the ClientConnector to connect using the new arguments.
     */
   status_t Start(const String & signaturePattern, const String & systemNamePattern, const ConstQueryFilterRef & optAdditionalDiscoveryCriteria = ConstQueryFilterRef(), uint64 autoReconnectTimeMicroseconds = MillisToMicros(250));

   /** Stops the network I/O thread, if it is currently running. */
   void Stop();

   /** Returns true if this connector's I/O thread is currently started, or false if it is not. */
   bool IsActive() const;

   /** Convenience method:  Returns true iff our TCP connection to the server is currently established */
   bool IsConnected() const {return (_connectedPeerInfo() != NULL);}

   /** Returns a Message containing information about the peer we are currently connected to, or a NULL Message if we aren't currently connected. */
   MessageRef GetConnectedPeerInfo() const {return _connectedPeerInfo;}

   /** Returns the signature--pattern string that was passed in to our Start() method, or an empty String if we aren't currently started. */
   const String & GetSignaturePattern() const;

   /** Returns the system-name-pattern string that was passed in to our Start() method, or an empty String if we aren't currently started. */
   const String & GetSystemNamePattern() const;

   /** Returns a reference to the QueryFilter object that was previously passed in to our Start() method, or a NULL reference if we aren't currently started. */
   const ConstQueryFilterRef & GetAdditionalDiscoveryCriteria() const;

   /** Returns automatic-reconnect-delay that previously passed in to our Start() method, or 0 if we aren't currently started. */
   uint64 GetAutoReconnectTimeMicroseconds() const;

   /** Returns the local clock-time (as per GetRunTime64()) when we last received a time-sync-pong from our connected server.
     * Returns MUSCLE_TIME_NEVER if we have never received time-sync-pong so far during this connection.
     */
   uint64 GetTimeOfLastTimeSyncPong() const {return _mainThreadLastTimeSyncPongTime;}

   // INetworkTimeProvider API
   virtual uint64 GetNetworkTime64() const {return GetNetworkTime64ForRunTime64(GetRunTime64());}

   virtual uint64 GetRunTime64ForNetworkTime64(uint64 networkTime64TimeStamp) const
   {
      const int64 ntto = _mainThreadToNetworkTimeOffset;  // capture local copy of atomic, to avoid race conditions
      return ((ntto==INVALID_TIME_OFFSET)||(networkTime64TimeStamp==MUSCLE_TIME_NEVER))?MUSCLE_TIME_NEVER:(networkTime64TimeStamp-ntto);
   }

   virtual uint64 GetNetworkTime64ForRunTime64(uint64 runTime64TimeStamp) const
   {
      const int64 ntto = _mainThreadToNetworkTimeOffset;  // capture local copy of atomic, to avoid race conditions
      return ((ntto==INVALID_TIME_OFFSET)||(runTime64TimeStamp==MUSCLE_TIME_NEVER))?MUSCLE_TIME_NEVER:(runTime64TimeStamp+ntto);
   }

   virtual int64 GetToNetworkTimeOffset() const {return _mainThreadToNetworkTimeOffset;}

protected:
   virtual void DispatchCallbacks(uint32 eventTypeBits);

   /** May be overridden with a custom method for extracting the TCP port-number to connect to from the given discovery-results Message.
     * @param msg The Message to parse
     * @param retPort on success, the TCP port number should be written here.
     * @returns B_NO_ERROR on success, or some other error code on failure.
     * @note the default implementation just calls msg.FindInt16("port", retPort)
     * @note this method will be called from the network I/O thread, so be careful about accessing any data other than the passed-in arguments.
     */
   virtual status_t ParseTCPPortFromMessage(const Message & msg, uint16 & retPort) const;

   /** Called when our connection-status changes.
     * @param optServerInfo if non-NULL, information about the ZG server we have connected to.
     *                      If we have just disconnected, then this will be a NULL MessageRef().
     */
   virtual void ConnectionStatusUpdated(const MessageRef & optServerInfo) = 0;

   /** Called when a Message has been received from the TCP connection
     * @param msg Reference to the received Message
     */
   virtual void MessageReceivedFromNetwork(const MessageRef & msg) = 0;

   // INetworkMessageSender API -- sends (msg) out over our TCP connection
   virtual status_t SendOutgoingMessageToNetwork(const MessageRef & msg);

private:
   friend class ClientConnectorImplementation;

   void TimeSyncReceived(uint64 roundTripTime, uint64 serverNetworkTime, uint64 localReceiveTime);
   void MessageReceivedFromIOThread(const MessageRef & msg);  // called by I/O thread!

   ClientConnectorImplementation * _imp;
   MessageRef _connectedPeerInfo;

   Queue<MessageRef> _scratchQueue;

   Mutex _replyQueueMutex;
   Queue<MessageRef> _replyQueue;

   ZGTimeAverager _timeAverager;
   std::atomic<int64> _mainThreadToNetworkTimeOffset;
   std::atomic<uint64> _mainThreadLastTimeSyncPongTime;
};
DECLARE_REFTYPES(ClientConnector);

};  // end namespace zg

#endif

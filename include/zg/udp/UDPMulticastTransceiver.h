#ifndef UDPMulticastTransceiver_h
#define UDPMulticastTransceiver_h

#include "zg/callback/ICallbackSubscriber.h"
#include "util/ByteBuffer.h"
#include "util/RefCount.h"
#include "util/String.h"
#include "util/IPAddress.h"

namespace zg {

class IUDPMulticastNotificationTarget;
class UDPMulticastTransceiverImplementation;

/** This object allows you to send and receive low-latency IPv6 Multicast UDP
  * packets to other UDPMulticastTransceiver objects on the same LAN.
  */
class UDPMulticastTransceiver : public ICallbackSubscriber, public RefCountable
{
public:
   /** Constructor.
     * @param mechanism the CallbackMechanism we should use to call callback-methods in the main thread
     * @note be sure to call Start() to start the transceiver-thread running!
     */
   UDPMulticastTransceiver(ICallbackMechanism * mechanism);

   /** Destructor */
   ~UDPMulticastTransceiver();

   /** Starts the transceiver thread going.
     * @param transmissionKey a string that will be hashed to compute an IPv6 multicast address to use.
     *                        Passing in the ZG system name is a recommended usage here.
     * @param perSenderMaxBacklogDepth How many received packets we should allow to queue up from each
     *                        remote location at a time, before we start dropping old packets.  (Note that
     *                        this only matters if your main thread isn't fast enough to handle the incoming
     *                        UDP packets as fast as they are received).  Defaults to 1.
     *                        Set this to zero to disable reception of incoming multicast packets entirely
     *                        (in which case the UDPMulticastTransceiver will be send-only)
     * @returns B_NO_ERROR on success, or an error code if setup failed.
     * @note if called while the thread is already running, the thread will be stopped and then restarted.
     */
   status_t Start(const String & transmissionKey, uint32 perSenderMaxBacklogDepth=1);

   /** Stops the transceiver thread, if it is currently running. */
   void Stop();

   /** Returns true if this module's transceiver thread is currently started/active, or false if it is not. */
   bool IsActive() const {return _isActive;}

   /** Call this to send out a multicast UDP packet that will be received by all other UDPMulticastTransceiver
     * objects on the LAN that are using the same transmission key as we are.
     * @param payloadBytes the payload bytes to put into the multicast UDP packet
     * @returns B_NO_ERROR on success, or some other value on error.
     */
   status_t SendMulticastPacket(const ByteBufferRef & payloadBytes);

   /** Call this to send out a unicast UDP packet that will be received whoever is
     * receiving UDP packets at the specified location.
     * @param targetAddress the IP address and port that the unicast packet should be sent to.
     * @param payloadBytes the payload bytes to put into the multicast UDP packet
     * @returns B_NO_ERROR on success, or some other value on error.
     */
   status_t SendUnicastPacket(const IPAddressAndPort & targetAddress, const ByteBufferRef & payloadBytes);

   /** Returns the current transmission-key string we are using (as specified previously in your call to Start()). 
     *  Only valid if we are currently active.
     */ 
   const String & GetTransmissionKey() const {return _transmissionKey;}

   /** Returns our current per-sender maximum backlog depth (as specified previously in your call to Start()).
     * Only valid if we are currently active. 
     */
   uint32 GetPerSeconderMaxBacklogDepth() const {return _perSenderMaxBacklogDepth;}

protected:
   virtual void DispatchCallbacks(uint32 eventTypeBits);

private:
   friend class IUDPMulticastNotificationTarget;
   friend class UDPMulticastTransceiverImplementation;

   void RegisterTarget(IUDPMulticastNotificationTarget * newTarget);
   void UnregisterTarget(IUDPMulticastNotificationTarget * target);
   void MainThreadNotifyAllOfSleepChange(bool isAboutToSleep);

   String _transmissionKey;
   uint32 _perSenderMaxBacklogDepth;
   bool _isActive;

   Hashtable<IUDPMulticastNotificationTarget *, Void> _targets;

   UDPMulticastTransceiverImplementation * _imp;
};
DECLARE_REFTYPES(UDPMulticastTransceiver);

};  // end namespace zg

#endif

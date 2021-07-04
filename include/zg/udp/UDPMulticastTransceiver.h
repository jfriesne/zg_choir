#ifndef UDPMulticastTransceiver_h
#define UDPMulticastTransceiver_h

#include "zg/callback/ICallbackSubscriber.h"
#include "zg/ZGPeerSettings.h"  // for ZG_MULTICAST_BEHAVIOR_*
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

   /** Call this to send out a UDP packet to the specified target address-and-port.
     * @param targetAddress the IP address and port that the UDP packet should be sent to.
     * @param payloadBytes the payload bytes to put into the UDP packet
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
   uint32 GetPerSenderMaxBacklogDepth() const {return _perSenderMaxBacklogDepth;}

   /** Specify what kind of multicast behavior this UDPMulticastTransceiver should use.
     * @param whichBehavior a ZG_MULTICAST_BEHAVIOR_* value.  (Default state is ZG_MULTICAST_BEHAVIOR_AUTO)
     * @note the new behavior won't take effect until the next time Start() is called.
     */
   void SetMulticastBehavior(uint32 whichBehavior) {_multicastBehavior = whichBehavior;}

   /** Returns our current multicast-behavior setting.  Note that this value may not correspond
     * to what is actually being used, during the period between when you called SetMulticastBehavior()
     * and when you next called Start().
     */
   uint32 GetMulticastBehavior() const {return _multicastBehavior;}

   /** If you want to restrict which network interfaces are to be used for sending and receiving
     * multicast packets, you can call this method before calling Start().
     * @param nameFilter wildcard-able expression indicating which network interfaces may be used.
     *                   For example, "en*" would specify only network interfaces whose names start with "en",
     *                   or "en1,en2,lo0" would specify only those three devices.  If set to an empty string
     *                   (which is the default state) then no filtering is done and all applicable interfaces
     *                   will be used.
     * @note the new filter string will not be applied until the next call to Start().
     */
   void SetNetworkInterfaceNameFilter(const String & nameFilter) {_nicNameFilter = nameFilter;}

   /** Returns the current network-interface-names filter, as previously passed to SetNetworkInterfaceNameFilter().
     * Default value is "" (aka no filtering enabled)
     */
   const String & GetNetworkInterfaceNameFilter() const {return _nicNameFilter;}

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
   uint32 _multicastBehavior;
   String _nicNameFilter;
   bool _isActive;

   Hashtable<IUDPMulticastNotificationTarget *, Void> _targets;

   UDPMulticastTransceiverImplementation * _imp;
};
DECLARE_REFTYPES(UDPMulticastTransceiver);

};  // end namespace zg

#endif

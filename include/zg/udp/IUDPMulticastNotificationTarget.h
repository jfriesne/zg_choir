#ifndef IUDPMulticastNotificationTarget_h
#define IUDPMulticastNotificationTarget_h

#include "message/Message.h"
#include "zg/ZGNameSpace.h"
#include "util/IPAddress.h"

namespace zg {

class UDPMulticastTransceiver;

/** Interface for an object that wants to be notified about incoming multicast UDP packets from a UDPMulticastTransceiver */
class IUDPMulticastNotificationTarget
{
public:
   /** Constructor
     * param multicastTransceiver pointer to the UDPMulticastTransceiver to register with, or NULL if you want to call SetUDPMulticastTransceiver() explicitely later.
     */
   IUDPMulticastNotificationTarget(UDPMulticastTransceiver * multicastTransceiver) : _multicastTransceiver(NULL) {SetUDPMulticastTransceiver(multicastTransceiver);}

   /** Destructor.  Calls SetUDPMulticastTransceiver(NULL) to auto-unregister this object from the UDPMulticastTransceiver if necessary. */
   virtual ~IUDPMulticastNotificationTarget() {SetUDPMulticastTransceiver(NULL);}

   /** Unregisters this object from any UDPMulticastTransceiver it's currently registered to
     * (if necessary) and then registers it with (multicastTransceiver).
     * @param multicastTransceiver pointer to the UDPMulticastTransceiver to register with, or NULL to unregister only.
     * @note if this IUDPMulticastNotificationTarget is already registered with (multicastTransceiver),
     *       then no action is taken.
     */
   void SetUDPMulticastTransceiver(UDPMulticastTransceiver * multicastTransceiver);

   /** Returns a pointer to the UDPMulticastTransceiver we're currently registered with, or NULL
     * if we aren't currently registered with any UDPMulticastTransceiver.
     */
   UDPMulticastTransceiver * GetUDPMulticastTransceiver() const {return _multicastTransceiver;}

   /** Called when a UDP multicast packet has been received from somewhere on the LAN.
     * @param sourceLocation IP address and port where the packet originated from
     * @param packetBytes the contents of the packet.
     */
   virtual void UDPPacketReceived(const IPAddressAndPort & sourceLocation, const ByteBufferRef & packetBytes) = 0;

   /** Called just before this computer goes into sleep mode.  Default implementation is a no-op. */
   virtual void ComputerIsAboutToSleep() {/* empty */}

   /** Called just after this computer comes out of sleep mode.  Default implementation is a no-op. */
   virtual void ComputerJustWokeUp() {/* empty */}

   /** Convenience method:  Calls SendMulticastPacket() on the UDPMulticastTransceiver object we are registered with.
     * @param payloadBytes the UDP payload bytes to send via multicast
     * @returns the value returned by UDPMulticastTransceiver::SendMulticastPacket(payloadBytes), or B_BAD_OBJECT if we aren't currently registered with one.
     */
   status_t SendMulticastPacket(const ByteBufferRef & payloadBytes);

   /** Convenience method:  Calls SendUnicastPacket() on the UDPMulticastTransceiver object we are registered with.
     * @param payloadBytes the UDP payload bytes to send in the packet
     * @param targetAddress the IP address and port to send the UDP packet to (perhaps as previously passed to you in a UDPPacketReceived() call)
     * @returns the value returned by UDPMulticastTransceiver::SendUnicastPacket(targetAddress, payloadBytes), or B_BAD_OBJECT if we aren't currently registered with one.
     */
   status_t SendUnicastPacket(const IPAddressAndPort & targetAddress, const ByteBufferRef & payloadBytes);

private:
   UDPMulticastTransceiver * _multicastTransceiver;
};

};  // end namespace zg

#endif

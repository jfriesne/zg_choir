#ifndef DiscoveryServerSession_h
#define DiscoveryServerSession_h

#include "zg/discovery/common/DiscoveryUtilityFunctions.h"
#include "reflector/AbstractReflectSession.h"

namespace zg {

class IDiscoveryServerSessionController;

/** This session listens for multicast query packets from clients, and when it receives one
 *  that matches our criteria, it wll send a unicast reply UDP packet back to the client to
 *  notify it of our existence and our information.
 */
class DiscoveryServerSession : public AbstractReflectSession
{
public:
   /** Constructor 
     * @param master IDiscoverySessionController to get our pong Messages from.
     * @param discoveryPort the port that we should listen for incoming discovery-query UDP packets on.  Defaults to DEFAULT_ZG_DISCOVERY_PORT.
     */
   DiscoveryServerSession(IDiscoveryServerSessionController & master, uint16 discoveryPort = DEFAULT_ZG_DISCOVERY_PORT);

   /** Destructor */
   virtual ~DiscoveryServerSession();

   /** Overridden to allocate our buffers and launch our watch-interfaces session */
   virtual status_t AttachedToServer();

   /** Overridden to release our watch-interfaces session */
   virtual void AboutToDetachFromServer();

   /** Overridden to create a UDP multicast socket */
   virtual ConstSocketRef CreateDefaultSocket();

   /** Returns true iff we have data pending, or any of our subscribers have notified us
    *  that they have data they want to send. 
    */
   virtual bool HasBytesToOutput() const {return (_outputData.HasItems());}

   // Overridden to read UDP data from our UDP socket directly
   virtual int32 DoInput(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes);

   // Overridden to send UDP data to our UDP socket directly
   virtual int32 DoOutput(uint32 maxBytes);

   // Overridden to do nothing -- we don't care about Messages from our neighbors!
   virtual void MessageReceivedFromSession(AbstractReflectSession &, const MessageRef &, void *);

   /** Call this to have us forget our master point (useful e.g. if the master is about to be deleted) */
   void ForgetBroadcastMaster() {_master = NULL;}

   /** Implemented as a no-op (we intercept DoInput() directly instead of relying on an AbstractMessageIOGateway() anyway) */
   virtual void MessageReceivedFromGateway(const muscle::MessageRef&, void*) {/* empty */}

   /** Will be called when the current set of active network interfaces has changed. */
   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames);

   virtual uint64 GetPulseTime(const PulseArgs & args);
   virtual void Pulse(const PulseArgs & args);

private:
   void HandlePing(const MessageRef & msg, const IPAddressAndPort & sourceIAP);
   void ScheduleSendPong(const MessageRef & pongMsg, const IPAddressAndPort & destIAP);

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
   class UDPReply
   {
   public:
      UDPReply() {/* empty */}
      UDPReply(uint64 sendTime, const MessageRef & data) : _sendTime(sendTime), _data(data) {/* empty */}
   
      uint64 GetSendTime() const {return _sendTime;}
      MessageRef GetData() const {return _data;}

      bool operator < (const UDPReply & rhs) const {return _sendTime < rhs._sendTime;}
      bool operator == (const UDPReply & rhs) const {return _sendTime == rhs._sendTime;}  // deliberately ignoring the _data field

   private:
      uint64 _sendTime;
      MessageRef _data;
   };
#endif

   const uint16 _discoveryPort;
   IDiscoveryServerSessionController * _master;
   Hashtable<IPAddressAndPort, UDPReply> _outputData;  // data ready to be sent out via UDP, ASAP
   OrderedValuesHashtable<IPAddressAndPort, UDPReply> _queuedOutputData;  // data waiting for the right time to be added to the _outputData Queue.
   ByteBufferRef _receiveBuffer;
   AbstractReflectSessionRef _watchInterfacesSession;
};
DECLARE_REFTYPES(DiscoveryServerSession);

};  // end namespace zg

#endif

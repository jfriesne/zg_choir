#ifndef ServerSideNetworkTreeGatewaySubscriber_h
#define ServerSideNetworkTreeGatewaySubscriber_h

#include "zg/messagetree/gateway/ProxyTreeGateway.h"

namespace zg {

class INetworkMessageSender;

/** Server-side gateway-subscriber class; parses incoming incoming NTG_COMMAND_* Messages from our TCP-connection to our client
  * and calls the appropriate ITreeGateway methods so that they will be acted upon by the upstream gateway.
  */
class ServerSideNetworkTreeGatewaySubscriber : public ITreeGatewaySubscriber
{
public:
   /** Constructor
     * @param upstreamGateway the ITreeGateway we will uses to access database operations
     * @param messageSender an object we can call SendOutgoingMessageToNetwork() when we want to send a Message back to our client via TCP.
     */
   ServerSideNetworkTreeGatewaySubscriber(ITreeGateway * upstreamGateway, INetworkMessageSender * messageSender);

   /**
     * To be called when a command-Message is received from our client via the TCP connection
     * @param msg the Message that was received
     * @returns B_NO_ERROR if the Message was handled, or B_UNIMPLEMENTED if the Message type was unknown, or some other error on miscellaneous failure.
     */
   virtual status_t IncomingTreeMessageReceivedFromClient(const MessageRef & msg);

   // ITreeGatewaySubscriber callback API -- implemented to create Message objects and send them back to our client over TCP
   virtual void TreeNodeUpdated(const String & nodePath, const ConstMessageRef & payloadMsg, const String & optOpTag);
   virtual void TreeNodeIndexCleared(const String & path, const String & optOpTag);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeLocalPeerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void MessageReceivedFromTreeSeniorPeer(int32 optWhichDB, const String & tag, const MessageRef & payload);
   virtual void MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);

private:
   void HandleIndexEntryUpdate(uint32 whatCode, const String & path, uint32 idx, const String & nodeName, const String & optOpTag);
   status_t SendOutgoingMessageToNetwork(const ConstMessageRef & msg);
   QueryFilterRef InstantiateQueryFilterAux(const Message & qfMsg, uint32 idx);

   INetworkMessageSender * _messageSender;
};

};  // end namespace zg

#endif

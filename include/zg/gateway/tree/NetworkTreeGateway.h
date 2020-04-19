#ifndef NetworkTreeGateway_h
#define NetworkTreeGateway_h

#include "zg/gateway/INetworkMessageSender.h"
#include "zg/gateway/tree/ProxyTreeGateway.h"

namespace zg {

/** Base class for both ClientSideNetworkTreeGateway and ServerSideNetworkTreeGateway classes */
class NetworkTreeGateway : public ProxyTreeGateway
{
protected:
   /** Constructor (protected because you need to instantiate a ClientSideNetworkTreeGateway, not just a NetworkTreeGateway)
     * @param optUpstreamGateway what upstream gateway to use.  (Set to NULL if we're on the client side and using TCP as our "upstream" rather than another gateway)
     * @param messageSender An object that will send Messages out over the TCP connection on our behalf.  Must not be NULL.
     */
   NetworkTreeGateway(ITreeGateway * optUpstreamGateway, INetworkMessageSender * messageSender);

public:
   virtual ~NetworkTreeGateway();

   void SetNetworkConnected(bool isConnected);
   bool IsNetworkConnect() const {return _isConnected;}

protected:
   // IGateway function-call API
   virtual void CommandBatchEnds();

   // ITreeGateway function-call API
   virtual bool TreeGateway_IsGatewayConnected() const {return _isConnected;}

protected:
   status_t SendOutgoingMessageToNetwork(const MessageRef & msgRef);

private:
   INetworkMessageSender * _messageSender;

   bool _isConnected;
   MessageRef _outgoingBatchMsg;  // non-NULL iff we are in a command-batch and assembling a batch-Message to send
};

/** This gateway is intended to be run on a client; its methods are implemented to create the appropriate Message objects and send them to the server over TCP.  */
class ClientSideNetworkTreeGateway : public NetworkTreeGateway
{
public:
   /** Constructor
     * @param optUpstreamGateway what upstream gateway to use.  (Set to NULL if we're on the client side and using TCP as our "upstream" rather than another gateway)
     * @param messageSender An object that will send Messages out over the TCP connection on our behalf.  Must not be NULL.
     */
   ClientSideNetworkTreeGateway(ITreeGateway * optUpstreamGateway, INetworkMessageSender * messageSender);

   virtual ~ClientSideNetworkTreeGateway();

protected:
   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore);
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const char * optBefore, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);

private:
   status_t HandleBasicCommandAux(uint32 what, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
};

/** Semi-abstract base class for both ClientSideNetworkTreeGatewaySubscriber and ServerSideNetworkTreeGatewaySubscriber classes
  */
class NetworkTreeGatewaySubscriber : public ITreeGatewaySubscriber
{
protected:
   /** Constructor (protected because you need to instantiate either a ClientSideNetworkTreeGatewaySubscriber or a ServerSideNetworkTreeGatewaySubscriber, not just a NetworkTreeGatewaySubscriber) */
   NetworkTreeGatewaySubscriber(ITreeGateway * optUpstreamGateway, INetworkMessageSender * messageSender) : ITreeGatewaySubscriber(optUpstreamGateway), _messageSender(messageSender) {/* empty */}

   status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return _messageSender->SendOutgoingMessageToNetwork(msg);}

private:
   INetworkMessageSender * _messageSender;
};

/** Semi-abstract client-side subscriber class, with a IncomingTreeMessageReceivedFromServer() method
  * to handle incoming NTG_REPLY_* Messages that were received from the server via TCP.
  */
class ClientSideNetworkTreeGatewaySubscriber : public NetworkTreeGatewaySubscriber
{
public:
   ClientSideNetworkTreeGatewaySubscriber(ITreeGateway * upstreamGateway, INetworkMessageSender * messageSender) : NetworkTreeGatewaySubscriber(upstreamGateway, messageSender) {/* empty */}

   /**
     * To be called when a reply-Message is received from our server via the TCP connection
     * @param msg the Message that was received
     * @returns B_NO_ERROR if the Message was handled, or B_UNIMPLEMENTED if the Message type was unknown, or some other error on miscellaneous failure.
     */
   virtual status_t IncomingTreeMessageReceivedFromServer(const MessageRef & msg);

private:
   status_t SendOutgoingMessageToNetwork(const MessageRef & msg) {return _messageSender->SendOutgoingMessageToNetwork(msg);}
   QueryFilterRef InstantiateQueryFilterAux(const Message & qfMsg, uint32 idx);

   INetworkMessageSender * _messageSender;
};

/** Semi-abstract server-side subscriber class, with a IncomingTreeMessageReceivedFromClient() method
  * to handle incoming NTG_COMMAND_* Messages that were received from a client via TCP.
  */
class ServerSideNetworkTreeGatewaySubscriber : public NetworkTreeGatewaySubscriber
{
public:
   ServerSideNetworkTreeGatewaySubscriber(ITreeGateway * upstreamGateway, INetworkMessageSender * messageSender) : NetworkTreeGatewaySubscriber(upstreamGateway, messageSender) {/* empty */}

   /**
     * To be called when a command-Message is received from our client via the TCP connection
     * @param msg the Message that was received
     * @returns B_NO_ERROR if the Message was handled, or B_UNIMPLEMENTED if the Message type was unknown, or some other error on miscellaneous failure.
     */
   virtual status_t IncomingTreeMessageReceivedFromClient(const MessageRef & msg);

   // ITreeGatewaySubscriber callback API -- implemented to create Message objecst and send them back to our client over TCP
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeServerPonged(const String & tag);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);

private:
   void HandleIndexEntryUpdate(uint32 whatCode, const String & path, uint32 idx, const String & nodeName);

   QueryFilterRef InstantiateQueryFilterAux(const Message & qfMsg, uint32 idx);
};

};  // end namespace zg

#endif

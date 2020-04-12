#ifndef NetworkTreeGateway_h
#define NetworkTreeGateway_h

#include "zg/gateway/tree/ProxyTreeGateway.h"

namespace zg {

/** This class just forwards on all requests from its downstream ITreeGatewaySubscribers to its upstream ITreeGateway,
  * and all replies from its upstream ITreeGateway back to its downstream ITreeGatewaySubscribers.
  * It's not that useful on its own, but rather is generally used as a starting point to subclass from.
  */
class NetworkTreeGateway : public ProxyTreeGateway
{
public:
   NetworkTreeGateway(ITreeGateway * optUpstreamGateway);

   virtual ~NetworkTreeGateway();

   void SetNetworkConnected(bool isConnected);
   bool IsNetworkConnect() const {return _isConnected;}

protected:
   // Must be implemented by subclass to send the specified MessageRef out to the network.
   virtual status_t SendOutgoingTreeMessageToNetwork(const MessageRef & msg) = 0;

   // IGateway function-call API
   virtual void CommandBatchEnds();

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
   virtual bool TreeGateway_IsGatewayConnected() const {return _isConnected;}

private:
   status_t AddOutgoingMessage(const MessageRef & msgRef);
   status_t HandleBasicCommandAux(uint32 what, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);

   bool _isConnected;
   MessageRef _outgoingBatchMsg;  // non-NULL iff we are in a command-batch and assembling a batch-Message to send
};

/** Semi-abstract subscriber class, with a IncomingTreeMessageReceivedFromClient() method
  * to handle incoming NTG_COMMAND_* Messages that were produced by a downstream NetworkTreeGateway
  */
class NetworkTreeGatewaySubscriber : public ITreeGatewaySubscriber
{
public:
   NetworkTreeGatewaySubscriber(ITreeGateway * gateway) : ITreeGatewaySubscriber(gateway) {/* empty */}

   /**
     * To be called (on the server) when a command-Message is received from our client via the TCP connection
     * @param msg the Message that was received
     * @returns B_NO_ERROR if the Message was handled, or B_UNIMPLEMENTED if the Message type was unknown, or some other error on miscellaneous failure.
     */
   virtual status_t IncomingTreeMessageReceivedFromClient(const MessageRef & msg);

   /**
     * To be called (on the client) when a reply-Message is received from our server via the TCP connection
     * @param msg the Message that was received
     * @returns B_NO_ERROR if the Message was handled, or B_UNIMPLEMENTED if the Message type was unknown, or some other error on miscellaneous failure.
     */
   virtual status_t IncomingTreeMessageReceivedFromServer(const MessageRef & msg);

   // ITreeGatewaySubscriber callback API
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeServerPonged(const String & tag);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);

protected:
   /** Subclass should implement this to send the specified Message back to our client via the TCP connection 
     * @param msg the Message to send to our client
     */
   virtual void SendOutgoingTreeMessageToClient(const MessageRef & msg) = 0;

private:
   QueryFilterRef InstantiateQueryFilterAux(const Message & qfMsg, uint32 idx);
   void HandleIndexEntryUpdate(uint32 whatCode, const String & path, uint32 idx, const String & nodeName);
};

};  // end namespace zg

#endif

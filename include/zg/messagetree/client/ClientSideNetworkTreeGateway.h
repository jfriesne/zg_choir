#ifndef ClientSideNetworkTreeGateway_h
#define ClientSideNetworkTreeGateway_h

#include "zg/messagetree/gateway/ProxyTreeGateway.h"

namespace zg {

class INetworkMessageSender;

/** This gateway is intended to be run on a client; its methods are implemented to create the appropriate Message objects and send them to the server over TCP.  */
class ClientSideNetworkTreeGateway : public ProxyTreeGateway
{
public:
   /** Constructor
     * @param messageSender An object that will send Messages out over the TCP connection on our behalf.  Must not be NULL.
     */
   ClientSideNetworkTreeGateway(INetworkMessageSender * messageSender);

   /** Destructor */
   virtual ~ClientSideNetworkTreeGateway();

   /** Called (typically by the MessageTreeClientConnector class) whenever the client connects to the server or disconnects from the server.
     * @param isConnected true if the TCP connection was just made, or false if it was just severed.
     */
   void SetNetworkConnected(bool isConnected);

   /** Returns true iff we are currently in TCP-connected mode (as specified by the most recent call to SetNetworkConnected()) */
   bool IsNetworkConnected() const {return _isConnected;}

   /**
     * Called (typically by the MessageTreeClientConnector that is managing us) when a reply-Message is received from our server via the TCP connection
     * @param msg the Message that was received
     * @returns B_NO_ERROR if the Message was handled, or B_UNIMPLEMENTED if the Message type was unknown, or some other error on miscellaneous failure.
     */
   virtual status_t IncomingTreeMessageReceivedFromServer(const MessageRef & msg);

protected:
   /** Overridden to send any pending batch-Message out to the network */
   virtual void CommandBatchEnds();

   // ITreeGateway function-call API
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore);
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const String * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags);
   virtual status_t TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB);
   virtual status_t TreeGateway_EndUndoSequence(  ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB);
   virtual status_t TreeGateway_RequestUndo(ITreeGatewaySubscriber * calledBy, uint32 whichDB);
   virtual status_t TreeGateway_RequestRedo(ITreeGatewaySubscriber * calledBy, uint32 whichDB);
   virtual bool TreeGateway_IsGatewayConnected() const {return _isConnected;}

protected:
   status_t SendOutgoingMessageToNetwork(const MessageRef & msgRef);

private:
   status_t HandleBasicCommandAux(uint32 what, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   status_t PingLocalPeerAux(const String & tag, int32 optWhichDB, TreeGatewayFlags flags);
   status_t IncomingMuscledMessageReceivedFromServer(const MessageRef & msg);
   status_t ConvertPathToSessionRelative(String & path) const;
   status_t SendUndoRedoMessage(uint32 whatCode, const String & tag, uint32 whichDB);

   INetworkMessageSender * _messageSender;
   bool _isConnected;
   MessageRef _outgoingBatchMsg;  // non-NULL iff we are in a command-batch and assembling a batch-Message to send
};

};  // end namespace zg

#endif

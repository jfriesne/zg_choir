#ifndef MessageTreeDatabasePeerSession_h
#define MessageTreeDatabasePeerSession_h

#include "zg/ZGDatabasePeerSession.h"
#include "zg/IDatabaseObject.h"
#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "zg/messagetree/gateway/ProxyTreeGateway.h"
#include "util/NestCount.h"

namespace zg
{

class ClientDataMessageTreeDatabaseObject;
class MessageTreeDatabaseObject;
class ServerSideMessageTreeSession;

/** This is a ZGDatabasePeerSession that has been further specialized to be able to
  * support MUSCLE node-tree database semantics in particular.
  */
class MessageTreeDatabasePeerSession : public ZGDatabasePeerSession, private ProxyTreeGateway
{
public:
   /** Constructor
     * @param peerSettings the ZGPeerSettings that this system is to use.
     */
   MessageTreeDatabasePeerSession(const ZGPeerSettings & peerSettings);

   /** Returns a pointer to the ITreeGateway object that our clients should use to access the ZG-synchronized database data */
   ITreeGateway * GetClientTreeGateway() {return &_muxGateway;}

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();

   /** Returns a pointer to the ServerSideMessageTreeSession object whose MessageReceivedFromGateway() callback we
     * are currently processing, or NULL if we aren't in that context.
     */
   ServerSideMessageTreeSession * GetActiveServerSideMessageTreeSession() const;

   /** Called by the ServerSideMessageTreeSession when it is about to detach from the server.
     * This call allows us to notify any ClientDataMessageTreeDatabaseObjects so they can
     * delete any shared nodes that correspond to it.
     */
   void ServerSideMessageTreeSessionIsDetaching(ServerSideMessageTreeSession * clientSession);

protected:
   /** This will be called as part of the startup sequence.  It should create
     * a new IDatabaseObject that will represent the specified database and return
     * a reference to it, for the MessageTreeDatabasePeerSession to manage.
     * @param whichDatabase The index of the database that we need an object to represent.
     */
   virtual IDatabaseObjectRef CreateDatabaseObject(uint32 whichDatabase) = 0;

   /** Overridden to call PushSubscriptionMessages() so that MUSCLE updates will go out in a timely manner */
   virtual void CommandBatchEnds();

   /** Given a nodePath, returns the most-closely associated MessageTreeDatabaseObject, or NULL if none matches.
     * @param nodePath a node-path to check (either absolute or session-relative)
     * @param optRetRelativePath if non-NULL, then on success, a database-object-relative sub-path will be written here.
     * @returns a pointer to the appropriate MessageTreeDatabaseObject on success, or NULL on failure (no matching DB found)
     */
   MessageTreeDatabaseObject * GetDatabaseForNodePath(const String & nodePath, String * optRetRelativePath) const;

   /** Given a nodePath, returns a table of all associated MessageTreeDatabaseObjects and their respective sub-paths.
     * @param nodePath a node-path to check (either absolute or session-relative; wildcards okay)
     * @returns a table of matching MessageTreeDatabaseObjects, each paired with the sub-path it should use.
     */
   Hashtable<MessageTreeDatabaseObject *, String> GetDatabasesForNodePath(const String & nodePath) const;

   /** Called after an ITreeGatewaySubscriber somewhere has called SendMessageToTreeSeniorPeer().
     * @param fromPeerID the ID of the ZGPeer that the subscriber is directly connected to
     * @param payload the Message that the subscriber sent to us
     * @param whichDB the database-object index that the subscriber indicated the Message is related to
     * @param tag a tag-string that can be used to route replies back to the originating subscriber, if desired.
     * @note Default implementation just calls through to the MessageReceivedFromTreeGatewaySubscriber() method of the
     *       specified MessageTreeDatabaseObject, or logs an error message if the specified MessageTreeDatabaseObject doesn't exist.
     */
   virtual void MessageReceivedFromTreeGatewaySubscriber(const ZGPeerID & fromPeerID, const MessageRef & payload, uint32 whichDB, const String & tag);

   /** Call this to send a Message back to an ITreeGatewaySubscriber (e.g. in response to a MessageReceivedFromTreeGatewaySubscriber() callback)
     * @param toPeerID the ID of the ZGPeer that the subscriber is directly connected to
     * @param tag the tag-String to use to direct the Message to the correct subscriber (as was previously passed in to MessageReceivedFromTreeGatewaySubscriber())
     * @param payload the Message to send to the subscriber
     * @param optWhichDB optional index of the database-object that this reply is coming from.  
     *                   Pass in -1 if the reply Message isn't meant to be associated with a particular database-object.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SendMessageToTreeGatewaySubscriber(const ZGPeerID & toPeerID, const String & tag, const MessageRef & payload, int32 optWhichDB);

   // ZGPeerSession API implementation
   virtual void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);

   // ITreeGateway API implementation
   virtual status_t TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags);
   virtual status_t TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore);
   virtual status_t TreeGateway_UploadNodeSubtree(ITreeGatewaySubscriber * calledBy, const String & basePath, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestDeleteNodes(ITreeGatewaySubscriber * calledBy, const String & path, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_RequestMoveIndexEntry(ITreeGatewaySubscriber * calledBy, const String & path, const String * optBefore, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags);
   virtual status_t TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags);
   virtual status_t TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * calledBy, const MessageRef & msg, uint32 whichDB, const String & tag);
   virtual status_t TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * calledBy, const String & subscriberPath, const MessageRef & msg, const ConstQueryFilterRef & optFilterRef, const String & tag);
   virtual status_t TreeGateway_BeginUndoSequence(ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB);
   virtual status_t TreeGateway_EndUndoSequence(  ITreeGatewaySubscriber * calledBy, const String & optSequenceLabel, uint32 whichDB);
   virtual status_t TreeGateway_RequestUndo(ITreeGatewaySubscriber * calledBy, uint32 whichDB);
   virtual status_t TreeGateway_RequestRedo(ITreeGatewaySubscriber * calledBy, uint32 whichDB);
   virtual bool TreeGateway_IsGatewayConnected() const {return IAmFullyAttached();}
   virtual ConstMessageRef TreeGateway_GetGestaltMessage() const;

   // StorageReflectSession API implementation
   virtual String GenerateHostName(const IPAddress &, const String &) const {return "zg";}
   virtual void NotifySubscribersThatNodeChanged(DataNode & node, const MessageRef & oldData, bool isBeingRemoved);
   virtual void NotifySubscribersThatNodeIndexChanged(DataNode & node, char op, uint32 index, const String & key);
   virtual void NodeChanged(DataNode & node, const MessageRef & oldData, bool isBeingRemoved);
   virtual void NodeIndexChanged(DataNode & node, char op, uint32 index, const String & key);

   // ZGPeerSession API implementation
   virtual ConstMessageRef SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg);
   virtual status_t JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg);
   virtual void MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg);

private:
   friend class MessageTreeDatabaseObject;
   friend class ClientDataMessageTreeDatabaseObject;
   friend class UndoStackMessageTreeDatabaseObject;

   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(MessageTreeDatabasePeerSession, GetPerClientPeerIDsCallback);   /** Peer IDs of matching nodes are added to the passed-in Hashtable */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(MessageTreeDatabasePeerSession, GetSubscribedSessionsCallback); /** ServerSideMessageTreeSessions that are subscribed to matching nodes are added to the passed-in Hashtable */

   ZGPeerID GetPerClientPeerIDForNode(const DataNode & node) const;
   status_t GetPerClientPeerIDsForPath(const String & path, const ConstQueryFilterRef & ref, Hashtable<ZGPeerID, Void> & retPeerIDs);

   status_t UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabel, uint32 whichDB);
   status_t GetUnusedNodeID(const String & path, uint32 & retID);
   status_t AddRemoveSubscriptionAux(uint32 whatCode, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags);
   void HandleSeniorPeerPingMessage(uint32 whichDatabase, const ConstMessageRef & msg);
   bool IsInSetupOrTeardown() const {return _inPeerSessionSetupOrTeardown.IsInBatch();}

   Queue<IDatabaseObjectRef> _databaseObjects;

   MuxTreeGateway _muxGateway;
   NestCount _inPeerSessionSetupOrTeardown;

   ConstMessageRef _gestaltMessage;
};
DECLARE_REFTYPES(MessageTreeDatabasePeerSession);

};  // end namespace zg

#endif

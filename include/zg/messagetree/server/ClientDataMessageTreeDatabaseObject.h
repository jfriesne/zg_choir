#ifndef ClientDataMessageTreeDatabaseObject_h
#define ClientDataMessageTreeDatabaseObject_h

#include "zg/messagetree/server/MessageTreeDatabaseObject.h"

namespace zg
{

class ServerSideMessageTreeSession;

/** This is a special subclass of MessageTreeDatabaseObject that can be used
  * to hold database state that is bound to a particular client connection.
  * Data posted by the client to this subtree will be tied to the lifetime
  * of the client's connection, and it will automatically get deleted when
  * the client disconnects from the system.  This can be useful for things 
  * like registries of currently-connected clients, their locations within
  * the system, and other data that is specific to a particular client-connection.
  */
class ClientDataMessageTreeDatabaseObject : public MessageTreeDatabaseObject 
{
public:
   /** Constructor
     * @param session pointer to the MessageTreeDatabasePeerSession object that created us
     * @param dbIndex our index within the databases-list.
     * @param rootNodePath a sub-path indicating where the root of our managed Message sub-tree
     *                     should be located (relative to the MessageTreeDatabasePeerSession's session-node)
     *                     May be empty if you want the session's session-node itself of be the
     *                     root of the managed sub-tree.
     */
   ClientDataMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath);

   /** Destructor */
   virtual ~ClientDataMessageTreeDatabaseObject() {/* empty */}

   // MessageTreeDatabaseObject API
   virtual status_t UploadNodeValue(const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const String * optBefore);
   virtual status_t UploadNodeSubtree(const String & path, const MessageRef & valuesMsg, TreeGatewayFlags flags);
   virtual status_t RequestDeleteNodes(const String & path, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags);
   virtual status_t RequestMoveIndexEntry(const String & path, const String * optBefore, const ConstQueryFilterRef & optFilter, TreeGatewayFlags flags);

   /** Called by the ServerSideMessageTreeSession when it is about to detach from the server.
     * This call allows us to delete any shared nodes that correspond to it.
     */
   void ServerSideMessageTreeSessionIsDetaching(ServerSideMessageTreeSession * clientSession);

protected:
   virtual void LocalSeniorPeerStatusChanged();
   virtual void PeerHasComeOnline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);
   virtual void PeerHasGoneOffline(const ZGPeerID & peerID, const ConstMessageRef & optPeerInfo);

private:
   String GetSharedPathFromLocalPath(const String & localPath, ServerSideMessageTreeSession * & retSessionNode) const; // given e.g. "foo/bar", returns "<peerid>/<ipaddress>/<sessionid>/foo/bar", suitable for sharing
   String GetSharedPathFromLocalPathAux(const String & localPath, ServerSideMessageTreeSession * ssmts) const;
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

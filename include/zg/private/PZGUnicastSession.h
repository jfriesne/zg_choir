#ifndef PZGUnicastSession_h
#define PZGUnicastSession_h

#include "reflector/AbstractReflectSession.h"
#include "zg/ZGPeerID.h"
#include "zg/private/PZGUpdateBackOrderKey.h"

namespace zg_private
{

class PZGNetworkIOSession;

/** This session represents a single TCP connection between two peers. */
class PZGUnicastSession : public AbstractReflectSession
{
public:
   PZGUnicastSession(PZGNetworkIOSession * master, const ZGPeerID & remotePeerID);

   virtual ~PZGUnicastSession();

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void EndSession();
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *) ;

   virtual const char * GetTypeName() const {return "Unicast";}

   /** Note that this may return an invalid Peer ID if we don't know who is calling us yet */
   const ZGPeerID & GetRemotePeerID() const {return _remotePeerID;}

   status_t RequestBackOrderFromSeniorPeer(const PZGUpdateBackOrderKey & ubok);

private:
   void RegisterMyself();
   void UnregisterMyself(bool forGood);

   ZGPeerID _remotePeerID;
   PZGNetworkIOSession * _master;

   Hashtable<PZGUpdateBackOrderKey, Void> _backorders;
};
DECLARE_REFTYPES(PZGUnicastSession);

};  // end namespace zg_private

#endif // PZGUnicastSession_h

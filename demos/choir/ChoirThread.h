#ifndef ChoirThread_h
#define ChoirThread_h

#include "qtsupport/QMessageTransceiverThread.h"
#include "ChoirNameSpace.h"
#include "ChoirSession.h"

namespace choir {

/** This class contains the MUSCLE network I/O thread that has our ChoirSession inside it */
class ChoirThread : public QMessageTransceiverThread
{
public:
   /** Constructor
     * @param peerSettings ZG settings for this ZG peer, as passed to the ZGPeerSession constructor.
     */
   ChoirThread(const ZGPeerSettings & peerSettings);

   /** Returns a pointer to our INetworkTimeProvider object */
   const INetworkTimeProvider * GetNetworkTimeProvider() const {return _choirSession();}

   /** Returns a reference to our ZGPeerSettings object (as was passed to our constructor) */
   const ZGPeerSettings & GetLocalPeerSettings() const;

   /** Returns the local peer's ZGPeerID. */
   const ZGPeerID & GetLocalPeerID() const;

protected:
   /** Overridden to add a ChoirSession object to our thread's internal ReflectServer */
   virtual ReflectServerRef CreateReflectServer();

private:
   ChoirSessionRef _choirSession;
};

}; // end namespace choir

#endif

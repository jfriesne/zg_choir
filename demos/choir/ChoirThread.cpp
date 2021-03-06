#include "ChoirThread.h"
#include "ChoirSession.h"

namespace choir {

ChoirThread :: ChoirThread(const ZGPeerSettings & peerSettings) : _choirSession(new ChoirSession(peerSettings))
{
   // empty
}

ReflectServerRef ChoirThread :: CreateReflectServer()
{
   ReflectServerRef ret = QMessageTransceiverThread::CreateReflectServer();
   if (ret() == NULL) return ReflectServerRef();

   if (ret()->AddNewSession(_choirSession).IsError())
   {
      LogTime(MUSCLE_LOG_ERROR, "ChoirThread::CreateReflectServer):  Error adding ChoirSession!\n");
      ret()->Cleanup();  // to avoid an assertion failure when the ReflectServer object gets deleted!
      return ReflectServerRef();
   }

   return ret;
}

const ZGPeerSettings & ChoirThread :: GetLocalPeerSettings() const
{
   return _choirSession()->GetPeerSettings();
}

const ZGPeerID & ChoirThread :: GetLocalPeerID() const
{
   return _choirSession()->GetLocalPeerID();
}

}; // end namespace choir

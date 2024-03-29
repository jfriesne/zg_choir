#ifndef TestTreeGatewaySubscriber_h
#define TestTreeGatewaySubscriber_h

#include "zg/ITextCommandReceiver.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "util/TimeUtilityFunctions.h"

namespace zg {

/** A simple implementation of ITreeGatewaySubscriber used for testing from the command line (in tree_client.cpp and connetor_client.cpp)
  * Designed to parse the user's input on stdin, and to print debug-info (about what ITreeGatewaySubscriber callbacks were called) on stdout.
  */
class TestTreeGatewaySubscriber : public ITreeGatewaySubscriber, public ITextCommandReceiver
{
public:
   /** Constructor
     * @param gateway the ITreeGateway we will use for network access
     */
   TestTreeGatewaySubscriber(ITreeGateway * gateway);

   /** Destructor */
   virtual ~TestTreeGatewaySubscriber();

   // ITreeGatewaySubscriber API
   virtual void CallbackBatchBegins();
   virtual void CallbackBatchEnds();
   virtual void TreeNodeUpdated(const String & nodePath, const ConstMessageRef & payloadMsg, const String & optOpTag);
   virtual void TreeNodeIndexCleared(const String & path, const String & optOpTag);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag);
   virtual void TreeLocalPeerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void MessageReceivedFromTreeSeniorPeer(int32 optWhichDB, const String & tag, const MessageRef & payload);
   virtual void MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeGatewayShuttingDown();

   // ITextCommandReceiver API
   MUSCLE_NODISCARD virtual bool IsReadyForTextCommands() const;
   MUSCLE_NODISCARD virtual bool TextCommandReceived(const String & textStr);

private:
   String GenerateOpTag(String & retTag);

   int _opTagCounter;
};

};

#endif

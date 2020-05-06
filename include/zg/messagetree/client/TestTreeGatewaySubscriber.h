#ifndef TestTreeGatewaySubscriber_h
#define TestTreeGatewaySubscriber_h

#include "zg/ITextCommandReceiver.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

namespace zg {

/** A simple implementation of ITreeGatewaySubscriber used for testing from the command line (in tree_client.cpp and connetor_client.cpp)
  * Designed to parse the user's input on stdin, and to print debug-info (about what ITreeGatewaySubscriber callbacks were called) on stdout.
  */
class TestTreeGatewaySubscriber : public ITreeGatewaySubscriber, public ITextCommandReceiver
{
public:
   TestTreeGatewaySubscriber(ITreeGateway * gateway);
   virtual ~TestTreeGatewaySubscriber();

   // ITreeGatewaySubscriber API
   virtual void CallbackBatchBegins();
   virtual void CallbackBatchEnds();
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & payloadMsg);
   virtual void TreeNodeIndexCleared(const String & path);
   virtual void TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName);
   virtual void TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName);
   virtual void TreeServerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeGatewayShuttingDown();

   // ITextCommandReceiver API
   virtual bool IsReadyForTextCommands() const;
   virtual bool TextCommandReceived(const String & textStr);
};

};

#endif

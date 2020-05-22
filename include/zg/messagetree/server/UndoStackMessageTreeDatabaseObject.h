#ifndef UndoStackMessageTreeDatabaseObject_h
#define UndoStackMessageTreeDatabaseObject_h

#include "zg/messagetree/server/MessageTreeDatabaseObject.h"
#include "util/NestCount.h"

namespace zg
{

enum {
   UNDOSTACK_COMMAND_MESSAGE_PAIR = 1970168943, ///< 'undo' -- pair of equal-and-opposite "do" and "undo" sub-Messages, sent to junior peers
   UNDOSTACK_COMMAND_BEGINSEQUENCE,             ///< mark the beginning of a sequence of undo-able actions
   UNDOSTACK_COMMAND_ENDSEQUENCE,               ///< mark the end of a sequence of undo-able actions
   UNDOSTACK_COMMAND_UNDO,                      ///< request that the most recent sequence in a client's undo-stack be un-done
   UNDOSTACK_COMMAND_REDO,                      ///< request that the most recent sequence in a client's redo-stack be re-done
};

/** This is a special subclass of MessageTreeDatabaseObject that implements
  * a per-client undo/redo stack as part of its operation.  You can use this 
  * if you want to be able to support undo and redo operations as part of
  * your user interface.
  */
class UndoStackMessageTreeDatabaseObject : public MessageTreeDatabaseObject 
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
   UndoStackMessageTreeDatabaseObject(MessageTreeDatabasePeerSession * session, int32 dbIndex, const String & rootNodePath);

   /** Destructor */
   virtual ~UndoStackMessageTreeDatabaseObject() {/* empty */}

   /** Called by the client's local peer when it receives a call to BeginUndoSequence(), EndUndoSequence(), RequestUndo(), or RequestRedo()
     * This method's job is to send that request on to the senior peer for handling.
     * @param whatCode an UNDOSTACK_COMMAND_* value
     * @param optSequenceLabel an optional user-readable string describing the undo/redo-operation at hand.
     * @returns B_NO_ERROR on success, or some other error code on failure.
     */
   status_t UploadUndoRedoRequestToSeniorPeer(uint32 whatCode, const String & optSequenceLabel);

protected:
   // IDatabaseObject API
   virtual ConstMessageRef SeniorUpdate(const ConstMessageRef & seniorDoMsg);
   virtual status_t SeniorMessageTreeUpdate(const ConstMessageRef & msg);
   virtual status_t JuniorUpdate(const ConstMessageRef & juniorDoMsg);

   virtual status_t SeniorRecordNodeUpdateMessage(const String & relativePath, const MessageRef & oldPayload, const MessageRef & newPayload, MessageRef & assemblingMessage, bool prepend);
   virtual status_t SeniorRecordNodeIndexUpdateMessage(const String & relativePath, char op, uint32 index, const String & key, MessageRef & assemblingMessage, bool prepend);

   // Overridden to add undo-tags as necessary
   virtual status_t RequestReplaceDatabaseState(const MessageRef & newDatabaseStateMsg);
   virtual status_t RequestUpdateDatabaseState(const MessageRef & databaseUpdateMsg);

   /** Convenience method:  Returns the undo-identifier-key of the currently active client, or an empty String if unknown */
   const String & GetActiveClientUndoKey() const;

private:
   friend class ObsoleteSequencesQueryFilter;

   status_t SeniorMessageTreeUpdateAux(const ConstMessageRef & msg);

   MessageRef _assembledJuniorUndoMessage;
   NestCount _seniorMessageTreeUpdateNestCount;
};
DECLARE_REFTYPES(MessageTreeDatabaseObject);

};  // end namespace zg

#endif

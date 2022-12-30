#include "zg/messagetree/gateway/SymlinkLogicMuxTreeGateway.h"

namespace zg {

// A helper class that subscribes to the symlinked-to nodes and tells the SymlinkLogicMuxTreeGateway when they change
class SymlinkResolver : public ITreeGatewaySubscriber
{
public:
   SymlinkResolver(SymlinkLogicMuxTreeGateway * master, ITreeGateway * optUpstreamGateway)
      : ITreeGatewaySubscriber(optUpstreamGateway)
      , _master(master)
   {
      // empty
   }

   // Called whenever the SymlinkLogicMuxTreeGateway gets a TreeNodeUpdated() call.  We use it to intercept
   // nodes containing symlinks and redirect the SymlinkLogicMuxTreeGateway to see the pointed-to node's data instead
   bool FilterTreeNodeUpdate(const String & nodePath, const ConstMessageRef & payloadMsg, const String & optOpTag, const Hashtable<ITreeGatewaySubscriber *, Void> & initialModeSubscribers)
   {
      const String & oldSymLinkPath = _symlinks[nodePath];
      const String & newSymLinkPath = payloadMsg() ? payloadMsg()->GetString(SYMLINK_FIELD_NAME) : GetEmptyString();
      if (newSymLinkPath != oldSymLinkPath)
      {
         if (oldSymLinkPath.HasChars()) (void) RemoveSymlink(nodePath);
         if (newSymLinkPath.HasChars()) (void) PutSymlink(nodePath, newSymLinkPath);
      }

      if (newSymLinkPath.HasChars())
      {
         if (initialModeSubscribers.HasItems())
         {
            // Make sure any new subscribers to an already-subscribed-to symlink get an initial update (even though we didn't resubscribe to its target)
            const SymlinkState * ss = _reverseSymlinks.Get(newSymLinkPath);
            if ((ss)&&(ss->_payload())) TreeNodeUpdated(newSymLinkPath, ss->_payload, optOpTag);
         }

         return true; // true == don't pass this update on to the normal subscribers (we don't want them to see the symlink-node's payload)
      }
      else return false;  // false == pass this update up to the superclass as usual
   }

   // Called by the original MuxTreeGateway when one of the symlink-destination-paths we subscribed to (in PutSymlink()) has been updated
   virtual void TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & optOpTag)
   {
      // When we get the result of one of our subscriptions, feed it back to our master gateway's subscribers as if it was associated with the symlink-nodes they subscribed to
      SymlinkState * sstate = _reverseSymlinks.Get(nodePath);
      if (sstate)
      {
         sstate->_payload = optPayloadMsg;
         for (HashtableIterator<String, Void> iter(sstate->_sourcePaths); iter.HasData(); iter++)
            _master->MuxTreeGateway::TreeNodeUpdated(iter.GetKey(), sstate->_payload, optOpTag);
      }
   }

   virtual void TreeGatewayConnectionStateChanged()
   {
      if (IsTreeGatewayConnected() == false)
      {
         _symlinks.Clear();
         _reverseSymlinks.Clear();
         (void) RemoveAllTreeSubscriptions();  // since we don't know what the symlinks are and therefore don't know what to subscribe to anyway
      }
      ITreeGatewaySubscriber::TreeGatewayConnectionStateChanged();
   }

   bool HasSymlink(const String & nodePath) const {return _symlinks.ContainsKey(nodePath);}

private:
   SymlinkLogicMuxTreeGateway * _master;

   class SymlinkState
   {
   public:
      Hashtable<String, Void> _sourcePaths;  // set of source-paths known to be pointing to our destination-path
      ConstMessageRef _payload;              // ConstMessageRef associated with the destination node (if known)
   };

   Hashtable<String, String>              _symlinks; // symlink-source-path -> symlink-destination-path
   Hashtable<String, SymlinkState> _reverseSymlinks; // symlink-destination-path -> symlink-info

   status_t RemoveSymlink(const String & nodePath)
   {
      const String * symDest = _symlinks.Get(nodePath);
      if (symDest)
      {
         SymlinkState * sstate = _reverseSymlinks.Get(*symDest);
         if ((sstate)&&(sstate->_sourcePaths.Remove(nodePath).IsOK())&&(sstate->_sourcePaths.IsEmpty()))
         {
            (void) RemoveTreeSubscription(*symDest, ConstQueryFilterRef());
            (void) _reverseSymlinks.Remove(*symDest);
         }

         return _symlinks.Remove(nodePath);
      }
      else return B_DATA_NOT_FOUND;
   }

   status_t PutSymlink(const String & nodePath, const String & symDest)
   {
      MRETURN_ON_ERROR(_symlinks.Put(nodePath, symDest));

      status_t ret;
      SymlinkState * sstate = _reverseSymlinks.GetOrPut(symDest);
      if (sstate)
      {
         const bool hadEntries = sstate->_sourcePaths.HasItems();
         if ((sstate->_sourcePaths.PutWithDefault(nodePath).IsOK(ret))&&((hadEntries)||(AddTreeSubscription(symDest, ConstQueryFilterRef()).IsOK(ret))))
         {
            if ((hadEntries)&&(sstate->_payload())) _master->MuxTreeGateway::TreeNodeUpdated(nodePath, sstate->_payload, GetEmptyString());
            return ret;
         }
      }
      else ret = B_OUT_OF_MEMORY;

      // If we got here, something went wrong, so roll back any side-effects and return failure
      if ((sstate)&&(sstate->_sourcePaths.Remove(nodePath).IsOK())&&(sstate->_sourcePaths.IsEmpty())) (void) _reverseSymlinks.Remove(symDest);
      (void) _symlinks.Remove(nodePath);

      return ret;
   }
};

SymlinkLogicMuxTreeGateway :: SymlinkLogicMuxTreeGateway(ITreeGateway * optUpstreamGateway)
   : MuxTreeGateway(optUpstreamGateway)
   , _symlinkResolver(new SymlinkResolver(this, optUpstreamGateway))
{
   // empty
}

SymlinkLogicMuxTreeGateway :: ~SymlinkLogicMuxTreeGateway()
{
   delete _symlinkResolver;
}

void SymlinkLogicMuxTreeGateway :: TreeNodeUpdated(const String & nodePath, const ConstMessageRef & payloadMsg, const String & optOpTag)
{
   if (_symlinkResolver->FilterTreeNodeUpdate(nodePath, payloadMsg, optOpTag, GetSubscribersInInitialResultsMode()) == false) MuxTreeGateway::TreeNodeUpdated(nodePath, payloadMsg, optOpTag);
}

void SymlinkLogicMuxTreeGateway :: ReceivedPathDropped(ITreeGatewaySubscriber * calledBy, const String & receivedPath)
{
   MuxTreeGateway::ReceivedPathDropped(calledBy, receivedPath);
   if ((_inFlushSymlinkPaths.IsInBatch() == false)&&(_symlinkResolver->HasSymlink(receivedPath))) (void) _droppedSymlinkPaths.PutWithDefault(receivedPath);
}

status_t SymlinkLogicMuxTreeGateway :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   NestCountGuard ncg(_inRemoveSubscription);
   const status_t ret = MuxTreeGateway::TreeGateway_RemoveSubscription(calledBy, subscriptionPath, optFilterRef, flags);
   if (_inRemoveSubscription.IsOutermost()) FlushDroppedSymlinkPaths();
   return ret;
}

status_t SymlinkLogicMuxTreeGateway :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags flags)
{
   NestCountGuard ncg(_inRemoveSubscription);
   const status_t ret = MuxTreeGateway::TreeGateway_RemoveAllSubscriptions(calledBy, flags);
   if (_inRemoveSubscription.IsOutermost()) FlushDroppedSymlinkPaths();
   return ret;
}

void SymlinkLogicMuxTreeGateway :: FlushDroppedSymlinkPaths()
{
   // Let the SymlinkResolver know about any symlink-paths that nobody is subscribing too any longer, so it can stop supporting them
   NestCountGuard ncg(_inFlushSymlinkPaths);
   while(_droppedSymlinkPaths.HasItems())
   {
      const String & path = *_droppedSymlinkPaths.GetFirstKey();
      if (IsAnyoneSubscribedToPath(path) == false) (void) _symlinkResolver->FilterTreeNodeUpdate(path, MessageRef(), GetEmptyString(), GetSubscribersInInitialResultsMode());
      (void) _droppedSymlinkPaths.RemoveFirst();
   }
}

};

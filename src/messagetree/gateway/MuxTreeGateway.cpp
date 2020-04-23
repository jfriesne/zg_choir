#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "reflector/StorageReflectConstants.h"  // for INDEX_OP_*

namespace zg {

static String NybbleizePointer(const void * ptr)
{
   String ret; (void) ret.Prealloc(sizeof(ptr)*2);

   const uint8 * bytePtr = (const uint8 *)(&ptr);
   for (uint32 i=0; i<sizeof(ptr); i++)
   {
      uint8 byte = bytePtr[i];
      ret += ((byte >> 0) & 0x0F) + 'A';
      ret += ((byte >> 4) & 0x0F) + 'A';
   }
   return ret;
}

static String AddCallbackPointerPrefix(void * ptr, const String & s) {return s.Prepend(NybbleizePointer(ptr)+':');}

static void * DenybbleizePointer(const String & ascii)
{
   void * ptr = NULL;
   uint8 * bytePtr = (uint8 *)(&ptr);
   uint32 numBytes = sizeof(ptr);
   if (ascii.Length() >= numBytes*2)
   {
      for (uint32 i=0; i<numBytes; i++)
      {
         const uint8 loNybble = (ascii[(i*2)+0]-'A');
         const uint8 hiNybble = (ascii[(i*2)+1]-'A');
         bytePtr[i] = ((loNybble<<0)|(hiNybble<<4));
      }
   }
   return ptr;
}

void * ParseCallbackPointerPrefix(const String & s, String & retSuffix)
{
   const int32 colIdx = s.IndexOf(':');
   if (colIdx >= (int32)(sizeof(void *)*2))
   {
      retSuffix = s.Substring(colIdx+1);
      return DenybbleizePointer(s);
   }
   else return NULL;
}


MuxTreeGateway :: MuxTreeGateway(ITreeGateway * optUpstreamGateway)
   : ProxyTreeGateway(optUpstreamGateway)
   , _isConnected(false)
{
   // empty
}

MuxTreeGateway :: ~MuxTreeGateway()
{
   // empty
}

status_t MuxTreeGateway :: TreeGateway_AddSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   TreeSubscriberInfoRef hisSubs;
   if (_subscriberInfos.Get(calledBy, hisSubs) != B_NO_ERROR) return B_BAD_ARGUMENT;  // he has to be a subscriber of ours!

   Queue<SubscriptionInfo> * filterQueue = _subscribedStrings.GetOrPut(subscriptionPath);
   if (filterQueue)
   {
      status_t ret;
      if (filterQueue->AddTail(SubscriptionInfo(calledBy, optFilterRef, flags)).IsOK(ret))
      {
         if (hisSubs() == NULL)
         {
            // demand-allocate an entry for him
            hisSubs.SetRef(newnothrow TreeSubscriberInfo);
            if ((hisSubs())&&(_subscriberInfos.Put(calledBy, hisSubs) != B_NO_ERROR)) hisSubs.Reset();
            if ((hisSubs())&&(_isConnected)) calledBy->TreeGatewayConnectionStateChanged();  // let him know we are connected to the server already
         }

         if (hisSubs())
         {
            if ((hisSubs()->PutPathString(subscriptionPath.WithoutPrefix("/"), optFilterRef).IsOK(ret))&&((_isConnected == false)||(UpdateSubscription(subscriptionPath, calledBy, flags).IsOK(ret)))) return B_NO_ERROR;
         }
         else ret = B_OUT_OF_MEMORY;

         // roll back (due to error)
         if ((hisSubs())&&(hisSubs()->GetEntries().IsEmpty())) _subscriberInfos.Put(calledBy, TreeSubscriberInfoRef());

         (void) filterQueue->RemoveTail();
         if (filterQueue->IsEmpty()) _subscribedStrings.Remove(subscriptionPath);
         return ret;
      }
      else return ret;
   }
   else RETURN_OUT_OF_MEMORY;
}

status_t MuxTreeGateway :: TreeGateway_RemoveSubscription(ITreeGatewaySubscriber * calledBy, const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags)
{
   Queue<SubscriptionInfo> * q = _subscribedStrings.Get(subscriptionPath);
   if (q)
   {
      int32 removeIdx = -1;
      for (uint32 i=0; i<q->GetNumItems(); i++)
      {
         const SubscriptionInfo & si = (*q)[i];
         if ((si.GetSubscriber() == calledBy)&&(si.GetQueryFilter() == optFilterRef)&&(flags == si.GetFlags()))
         {
            removeIdx = i;
            break;
         }
      }
      if ((removeIdx >= 0)&&(q->RemoveItemAt(removeIdx) == B_NO_ERROR))
      {
         (void) UpdateSubscription(subscriptionPath, calledBy, TreeGatewayFlags());  // this does the actual unsubscribe (or re-subscribe with a reduced queryfilter set, if necessary)
         if (q->IsEmpty()) (void) _subscribedStrings.Remove(subscriptionPath);
      }
      else return B_DATA_NOT_FOUND;  // hmm, unknown (subscriber/queryfilter) pair!

      TreeSubscriberInfoRef hisSubs;
      if ((_subscriberInfos.Get(calledBy, hisSubs) == B_NO_ERROR)&&(hisSubs()))
      {
         (void) hisSubs()->RemovePathString(subscriptionPath.WithoutSuffix("/"));
         if (hisSubs()->GetEntries().IsEmpty()) (void) _subscriberInfos.Put(calledBy, TreeSubscriberInfoRef());
      }
   }
   return B_NO_ERROR;
}

status_t MuxTreeGateway :: TreeGateway_RemoveAllSubscriptions(ITreeGatewaySubscriber * calledBy, TreeGatewayFlags /*flags*/)
{
   GatewaySubscriberCommandBatchGuard<ITreeGatewaySubscriber> batchGuard(calledBy);

   status_t ret;
   TreeSubscriberInfoRef subs;
   if ((_subscriberInfos.Get(calledBy, subs).IsOK(ret))&&(subs()))
   {
      const TreeSubscriberInfo temp = *subs(); // make local copy avoid potential re-entrancy issues
      for (HashtableIterator<String, PathMatcherEntry> iter(temp.GetEntries()); iter.HasData(); iter++)
         (void) TreeGateway_RemoveSubscription(calledBy, iter.GetKey(), iter.GetValue().GetFilter(), TreeGatewayFlags()).IsError(ret);  // IsError() will set (ret) on error
      return ret;
   }
   else return ret;
}

status_t MuxTreeGateway :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
   if (calledBy     == NULL)  return B_BAD_ARGUMENT;
   if (_isConnected == false) return B_BAD_OBJECT;

   // Demand-allocate a callback queue for our subscriber...
   Queue<String> * q = _requestedSubtrees.Get(calledBy);
   if (q == NULL)
   {
      (void) _requestedSubtrees.Put(calledBy, Queue<String>());
      q = _requestedSubtrees.Get(calledBy);
   }
   if (q)
   {
      status_t ret;
      if (q->AddTail(tag).IsOK(ret))
      {
         if (ITreeGatewaySubscriber::RequestTreeNodeSubtrees(queryStrings, queryFilters, AddCallbackPointerPrefix(calledBy, tag), maxDepth, flags).IsOK(ret)) return ret;
         q->RemoveTail();  // oops, roll back!
      }
      if (q->IsEmpty()) (void) _requestedSubtrees.Remove(calledBy);
      return ret;
   }
   else return B_OUT_OF_MEMORY;
}

status_t MuxTreeGateway :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const MessageRef & optPayload, TreeGatewayFlags flags, const char * optBefore)
{
   if (flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY)) TreeNodeUpdatedAux(path, optPayload, calledBy);  // if we won't get a reply back from the server, then we'll need to update our other subscribes directly.
   return ProxyTreeGateway::TreeGateway_UploadNodeValue(calledBy, path, optPayload, flags, optBefore);
}

status_t MuxTreeGateway :: TreeGateway_PingServer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags)
{
   return _isConnected ? ITreeGatewaySubscriber::PingTreeServer(AddCallbackPointerPrefix(calledBy, tag), flags) : B_BAD_OBJECT;
}

// Begin ITreeGatewaySubscriber callback API

void MuxTreeGateway :: TreeNodeUpdated(const String & path, const MessageRef & nodeMsg)
{
   TreeNodeUpdatedAux(path, nodeMsg, NULL);
}

void MuxTreeGateway :: TreeNodeUpdatedAux(const String & path, const MessageRef & msgRef, ITreeGatewaySubscriber * optDontNotify)
{
   if (_allowedCallbacks.HasItems())
   {
      // In this case, it's faster to iterate over just what's allowed
      for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(_allowedCallbacks); iter.HasData(); iter++)
      {
         ITreeGatewaySubscriber * sub = iter.GetKey();  // untrusted pointer
         TreeSubscriberInfo * subInfo = _subscriberInfos.GetWithDefault(sub)();
         if ((subInfo)&&(sub != optDontNotify)) UpdateSubscriber(sub, *subInfo, path, DoesPathMatch(sub, subInfo, path, msgRef()) ? msgRef : MessageRef());
      }
   }
   else
   {
      for (HashtableIterator<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> iter(_subscriberInfos); iter.HasData(); iter++)
      {
         ITreeGatewaySubscriber * sub = iter.GetKey();
         TreeSubscriberInfo * subInfo = iter.GetValue()();
         if ((subInfo)&&(sub != optDontNotify)) UpdateSubscriber(sub, *subInfo, path, DoesPathMatch(sub, subInfo, path, msgRef()) ? msgRef : MessageRef());
      }
   }
}

void MuxTreeGateway :: UpdateSubscriber(ITreeGatewaySubscriber * sub, TreeSubscriberInfo & subInfo, const String & path, const MessageRef & msgRef)
{
   if (msgRef())
   {
      EnsureSubscriberInBatchGroup(sub);
      (void) subInfo._receivedPaths.PutWithDefault(path);
      sub->TreeNodeUpdated(path, msgRef);
   }
   else if (subInfo._receivedPaths.Remove(path) == B_NO_ERROR)
   {
      EnsureSubscriberInBatchGroup(sub);
      sub->TreeNodeUpdated(path, msgRef);
   }
}

void MuxTreeGateway :: TreeNodeIndexCleared(const String & path)
{
   DoIndexNotifications(path, INDEX_OP_CLEARED, 0, GetEmptyString());
}

void MuxTreeGateway :: TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName)
{
   DoIndexNotifications(path, INDEX_OP_ENTRYINSERTED, insertedAtIndex, nodeName);
}

void MuxTreeGateway :: TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName)
{
   DoIndexNotifications(path, INDEX_OP_ENTRYREMOVED, removedAtIndex, nodeName);
}

void MuxTreeGateway :: DoIndexNotifications(const String & path, char opCode, uint32 index, const String & nodeName)
{
   if (_allowedCallbacks.HasItems())
   {
      // In this case, it's faster to iterate over just what's allowed
      for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(_allowedCallbacks); iter.HasData(); iter++)
      {
         ITreeGatewaySubscriber * sub = iter.GetKey();  // untrusted pointer
         TreeSubscriberInfoRef * pmr = _subscriberInfos.Get(sub);
         if ((pmr)&&(DoesPathMatch(sub, pmr->GetItemPointer(), path, NULL))) DoIndexNotificationAux(sub, path, opCode, index, nodeName);
      }
   }
   else
   {
      for (HashtableIterator<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> iter(_subscriberInfos); iter.HasData(); iter++)
         if (DoesPathMatch(iter.GetKey(), iter.GetValue()(), path, NULL)) DoIndexNotificationAux(iter.GetKey(), path, opCode, index, nodeName);
   }
}

void MuxTreeGateway :: DoIndexNotificationAux(ITreeGatewaySubscriber * sub, const String & path, char opCode, uint32 index, const String & nodeName)
{
   EnsureSubscriberInBatchGroup(sub);
   switch(opCode)
   {
      case INDEX_OP_CLEARED:       sub->TreeNodeIndexCleared(path);                        break;
      case INDEX_OP_ENTRYINSERTED: sub->TreeNodeIndexEntryInserted(path, index, nodeName); break;
      case INDEX_OP_ENTRYREMOVED:  sub->TreeNodeIndexEntryRemoved( path, index, nodeName); break;
   }
}


void MuxTreeGateway :: TreeServerPonged(const String & tag)
{
   String suffix;
   ITreeGatewaySubscriber * untrustedSubPtr = static_cast<ITreeGatewaySubscriber *>(ParseCallbackPointerPrefix(tag, suffix));
   if (_subscriberInfos.ContainsKey(untrustedSubPtr)) untrustedSubPtr ->TreeServerPonged(suffix);
}

void MuxTreeGateway :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   String suffix;
   ITreeGatewaySubscriber * untrustedSubPtr = static_cast<ITreeGatewaySubscriber *>(ParseCallbackPointerPrefix(tag, suffix));
   Queue<String> * q = _requestedSubtrees.Get(untrustedSubPtr);
   if (q)
   {
      if ((q)&&(q->RemoveFirstInstanceOf(suffix) == B_NO_ERROR))
      {
         untrustedSubPtr->SubtreesRequestResultReturned(suffix, subtreeData);
         if (q->IsEmpty()) _requestedSubtrees.Remove(untrustedSubPtr);
      }
   }
}

status_t MuxTreeGateway :: UpdateSubscription(const String & subscriptionPath, ITreeGatewaySubscriber * optSubscriber, TreeGatewayFlags flags)
{
   const Queue<SubscriptionInfo> * filterQueue = _subscribedStrings.Get(subscriptionPath);
   if ((filterQueue)&&(filterQueue->HasItems()))
   {
      const Queue<SubscriptionInfo> & q = *filterQueue;
      String obss; if (optSubscriber) obss = NybbleizePointer(optSubscriber);

      ConstQueryFilterRef sendFilter;  // what we will actually send
      ConstQueryFilterRef unionFilter; // the "matches against any of the following" meta-filter, demand-allocated
      bool useFilter = true;
      for (int32 i=q.GetNumItems()-1; i>=0; i--)
      {
         const SubscriptionInfo & nextItem = q[i];
         if (optSubscriber == NULL)
         {
            if (obss.HasChars()) obss += ',';
            obss += NybbleizePointer(nextItem.GetSubscriber());
         }

         if (useFilter)
         {
            const ConstQueryFilterRef & nextFilter = nextItem.GetQueryFilter();
            if (nextFilter())
            {
               if (sendFilter())
               {
                  OrQueryFilter * oqf = NULL;
                  if (unionFilter()) oqf = const_cast<OrQueryFilter *>(static_cast<const OrQueryFilter *>(unionFilter()));
                  else
                  {
                     oqf = newnothrow OrQueryFilter;
                     if (oqf)
                     {
                        unionFilter.SetRef(oqf);
                        (void) oqf->GetChildren().AddTail(sendFilter);
                        sendFilter = unionFilter;
                     }
                     else WARN_OUT_OF_MEMORY;
                  }
                  if (oqf) (void) oqf->GetChildren().AddTail(nextFilter);
               }
               else sendFilter = nextFilter;  // if there is only one filter, we can just use it verbatim, and avoid the overhead of a unionFilter
            }
            else
            {
               sendFilter.Reset();  // something OR everything == everything
               useFilter = false;   // no sense composing sub-filters any further, as we already know we'll be sending a command without any filter object specified
            }
         }
      }
      // TODO:  send obss somehow so that we'll know to filter the immediate results
      return ITreeGatewaySubscriber::AddTreeSubscription(subscriptionPath, sendFilter, flags);
   }
   else return ITreeGatewaySubscriber::RemoveTreeSubscription(subscriptionPath, ConstQueryFilterRef());
}

void MuxTreeGateway :: TreeGatewayConnectionStateChanged()
{
   const bool shouldBeConnected = ITreeGatewaySubscriber::IsTreeGatewayConnected();
   if (shouldBeConnected != _isConnected)
   {
      _isConnected = shouldBeConnected;

      GatewaySubscriberCommandBatchGuard<ITreeGatewaySubscriber> batchGuard(this);
      if (_isConnected)
      {
         // Re-add all our subscriptions to our upstream gateway, so we can re-sync all of our subscribers now that we're connected
         for (HashtableIterator<String, Queue<SubscriptionInfo> > iter(_subscribedStrings); iter.HasData(); iter++)
         {
            bool quiet = true;
            const Queue<SubscriptionInfo> & q = iter.GetValue();
            for (uint32 i=0; i<q.GetNumItems(); i++) if (q[i].GetFlags().IsBitSet(TREE_GATEWAY_FLAG_NOREPLY) == false) quiet = false;
            (void) UpdateSubscription(iter.GetKey(), NULL, quiet?TreeGatewayFlags(TREE_GATEWAY_FLAG_NOREPLY):TreeGatewayFlags());
         }
      }
      else
      {
         ITreeGatewaySubscriber::RemoveAllTreeSubscriptions();
         _requestedSubtrees.Clear();
      }

      ProxyTreeGateway::TreeGatewayConnectionStateChanged();  // notify our subscribers
   }
}

void MuxTreeGateway :: ShutdownGateway()
{
   ProxyTreeGateway::ShutdownGateway();
   _subscriberInfos.Clear();
   _needsCallbackBatchEndsCall.Clear();
   _subscribedStrings.Clear();
   _requestedSubtrees.Clear();
}

bool MuxTreeGateway :: DoesPathMatch(ITreeGatewaySubscriber * sub, const PathMatcher * pm, const String & path, const Message * optMessage) const
{
   return ((pm)&&((_allowedCallbacks.IsEmpty())||(_allowedCallbacks.ContainsKey(sub)))&&(pm->MatchesPath(path(), optMessage, NULL)));
}

void MuxTreeGateway :: EnsureSubscriberInBatchGroup(ITreeGatewaySubscriber * sub)
{
   if (_needsCallbackBatchEndsCall.ContainsKey(sub) == false)
   {
      (void) _needsCallbackBatchEndsCall.PutWithDefault(sub);
      sub->CallbackBatchBegins();
   }
}

void MuxTreeGateway :: CallbackBatchBegins()
{
   // deliberately empty; we'll tell our subscribers about the batch just before we call their one of their callback-methods for the first time
}

void MuxTreeGateway :: CallbackBatchEnds()
{
   // Notify everyone that we previously called CallbackBatchBegins() on, that that's all we have for them, for now
   while(_needsCallbackBatchEndsCall.HasItems())
   {
      ITreeGatewaySubscriber * a = *_needsCallbackBatchEndsCall.GetFirstKey();
      a->CallbackBatchEnds();
      (void) _needsCallbackBatchEndsCall.RemoveFirst();
   }

   _allowedCallbacks.Clear();
}

void MuxTreeGateway :: RegisterSubscriber(void * s)
{  
   ITreeGatewaySubscriber * sub = static_cast<ITreeGatewaySubscriber *>(s);
   ProxyTreeGateway::RegisterSubscriber(s);
   (void) _subscriberInfos.Put(sub, TreeSubscriberInfoRef());
}

void MuxTreeGateway :: UnregisterSubscriber(void * s)
{
   ITreeGatewaySubscriber * sub = static_cast<ITreeGatewaySubscriber *>(s);
   if (_subscriberInfos.ContainsKey(sub))
   {
      TreeGateway_RemoveAllSubscriptions(sub, TreeGatewayFlags());
      (void) _needsCallbackBatchEndsCall.Remove(sub);
      (void) _subscriberInfos.Remove(sub);
      (void) _requestedSubtrees.Remove(sub);
   }
   ProxyTreeGateway::UnregisterSubscriber(s);
}

};

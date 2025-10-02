#include "zg/messagetree/gateway/MuxTreeGateway.h"
#include "reflector/StorageReflectConstants.h"  // for INDEX_OP_*
#include "regex/SegmentedStringMatcher.h"
#include "util/StringTokenizer.h"

namespace zg {

String MuxTreeGateway :: GetRegistrationIDPrefix(ITreeGatewaySubscriber * sub, char markerChar) const
{
   char buf[128];
   muscleSprintf(buf, "%c" UINT32_FORMAT_SPEC "%c", markerChar, GetRegisteredSubscribers()[sub], markerChar);
   return buf;
}

String MuxTreeGateway :: PrependRegistrationIDPrefix(ITreeGatewaySubscriber * sub, const String & s, char markerChar) const
{
   return s.WithPrepend(GetRegistrationIDPrefix(sub, markerChar)+':');
}

ITreeGatewaySubscriber * MuxTreeGateway :: ParseRegistrationID(const String & ascii, char markerChar) const
{
   return ((ascii.StartsWith(markerChar))&&(ascii.EndsWith(markerChar))) ? GetRegistrationIDs()[atol(ascii()+1)] : NULL;
}

ITreeGatewaySubscriber * MuxTreeGateway :: ParseRegistrationIDPrefix(const String & s, String & retSuffix, char markerChar) const
{
   const int32 colIdx = s.IndexOf(':');
   if (colIdx >= 0)
   {
      retSuffix = s.Substring(colIdx+1);
      return ParseRegistrationID(s.Substring(0,colIdx), markerChar);
   }
   else return NULL;
}

static String GenerateUniqueMuxTreeGatewayIDPrefix()
{
   const uint64 uniqueID = (GetRunTime64()+GetCurrentTime64())*((uint64)rand());  // good enough for now?
   return String("[MUX%1]").Arg(uniqueID);
}

MuxTreeGateway :: MuxTreeGateway(ITreeGateway * optUpstreamGateway)
   : ProxyTreeGateway(optUpstreamGateway)
   , _muxTreeGatewayIDPrefix(GenerateUniqueMuxTreeGatewayIDPrefix())
   , _isConnected(false)
   , _dummySubscriber(NULL)
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
   if (_subscriberInfos.Get(calledBy, hisSubs).IsError()) return B_BAD_ARGUMENT;  // he has to be a subscriber of ours!

   Queue<SubscriptionInfo> * filterQueue = _subscribedStrings.GetOrPut(subscriptionPath);
   MRETURN_OOM_ON_NULL(filterQueue);

   MRETURN_ON_ERROR(filterQueue->AddTail(SubscriptionInfo(calledBy, optFilterRef, flags)));

   if (hisSubs() == NULL)
   {
      // demand-allocate an entry for him
      hisSubs.SetRef(new TreeSubscriberInfo);
      if (_subscriberInfos.Put(calledBy, hisSubs).IsError()) hisSubs.Reset();
      if ((hisSubs())&&(_isConnected)) calledBy->TreeGatewayConnectionStateChanged();  // let him know we are connected to the server already
   }

   status_t ret;
   if (hisSubs())
   {
      if ((hisSubs()->PutPathString(subscriptionPath.WithoutPrefix("/"), optFilterRef).IsOK(ret))&&((_isConnected == false)||(UpdateSubscription(subscriptionPath, calledBy, flags).IsOK(ret)))) return B_NO_ERROR;
   }
   else ret = B_OUT_OF_MEMORY;

   // roll back (due to error)
   if ((hisSubs())&&(hisSubs()->GetEntries().IsEmpty())) (void) _subscriberInfos.Put(calledBy, TreeSubscriberInfoRef());

   (void) filterQueue->RemoveTail();
   if (filterQueue->IsEmpty()) (void) _subscribedStrings.Remove(subscriptionPath);
   return ret;
}

void MuxTreeGateway :: ReceivedPathDropped(ITreeGatewaySubscriber * /*calledBy*/, const String & /*receivedPath*/)
{
   // empty
}

bool MuxTreeGateway :: IsAnyoneSubscribedToPath(const String & path) const
{
   for (HashtableIterator<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> iter(_subscriberInfos); iter.HasData(); iter++)
   {
      const TreeSubscriberInfo * tsi = iter.GetValue()();
      if ((tsi)&&(tsi->MatchesPath(path(), NULL, NULL))) return true;
   }
   return false;
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
      if ((removeIdx >= 0)&&(q->RemoveItemAt(removeIdx).IsOK()))
      {
         (void) UpdateSubscription(subscriptionPath, calledBy, TreeGatewayFlags());  // this does the actual unsubscribe (or re-subscribe with a reduced queryfilter set, if necessary)
         if (q->IsEmpty()) (void) _subscribedStrings.Remove(subscriptionPath);
      }
      else return B_DATA_NOT_FOUND;  // hmm, unknown (subscriber/queryfilter) pair!

      TreeSubscriberInfoRef hisSubs;
      if ((_subscriberInfos.Get(calledBy, hisSubs).IsOK())&&(hisSubs()))
      {
         for (HashtableIterator<String, uint32> iter(hisSubs()->_receivedPaths); iter.HasData(); iter++)
         {
            const String & receivedPath = iter.GetKey();
            if (hisSubs()->MatchesPath(receivedPath(), NULL, NULL))
            {
               ReceivedPathDropped(calledBy, receivedPath);
               (void) hisSubs()->_receivedPaths.Remove(receivedPath);
            }
         }

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
      const TreeSubscriberInfo temp = *subs(); // make local copy to avoid potential re-entrancy issues
      for (HashtableIterator<uint32, Hashtable<String, PathMatcherEntry > > iter(temp.GetEntries()); iter.HasData(); iter++)
         for (HashtableIterator<String, PathMatcherEntry> subIter(iter.GetValue()); subIter.HasData(); subIter++)
            (void) TreeGateway_RemoveSubscription(calledBy, subIter.GetKey(), subIter.GetValue().GetFilter(), TreeGatewayFlags()).IsError(ret);  // IsError() will set (ret) on error
      return ret;
   }
   else return ret;
}

status_t MuxTreeGateway :: TreeGateway_RequestNodeSubtrees(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags)
{
   return TreeGateway_RequestNodeSubtreesAux(calledBy, queryStrings, queryFilters, tag, maxDepth, flags, '_');
}

status_t MuxTreeGateway :: TreeGateway_RequestNodeValues(ITreeGatewaySubscriber * calledBy, const String & queryString, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, const String & tag)
{
   Queue<String> queryStrings;
   (void) queryStrings.AddTail(queryString);

   Queue<ConstQueryFilterRef> queryFilters;
   if (optFilterRef()) (void) queryFilters.AddTail(optFilterRef);

   return TreeGateway_RequestNodeSubtreesAux(calledBy, queryStrings, queryFilters, tag, 1, flags, '=');  // special markerChar '=' marks this request as coming from a RequestNodeValues() call
}

status_t MuxTreeGateway :: TreeGateway_RequestNodeSubtreesAux(ITreeGatewaySubscriber * calledBy, const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags, char markerChar)
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
         if (ITreeGatewaySubscriber::RequestTreeNodeSubtrees(queryStrings, queryFilters, PrependRegistrationIDPrefix(calledBy, tag, markerChar), maxDepth, flags).IsOK(ret)) return ret;
         (void) q->RemoveTail();  // oops, roll back!
      }
      if (q->IsEmpty()) (void) _requestedSubtrees.Remove(calledBy);
      return ret;
   }
   else return B_OUT_OF_MEMORY;
}

String MuxTreeGateway :: TagToExcludeClientFromReplies(ITreeGatewaySubscriber * excludeMe, const String & optOpTag) const
{
   return excludeMe ? PrependRegistrationIDPrefix(excludeMe, optOpTag, '!').WithPrepend(_muxTreeGatewayIDPrefix) : optOpTag;
}

status_t MuxTreeGateway :: TreeGateway_UploadNodeValue(ITreeGatewaySubscriber * calledBy, const String & path, const ConstMessageRef & optPayload, TreeGatewayFlags flags, const String & optBefore, const String & optOpTag)
{
   return ProxyTreeGateway::TreeGateway_UploadNodeValue(calledBy, path, optPayload, flags.WithoutBit(TREE_GATEWAY_FLAG_NOREPLY), optBefore, flags.IsBitSet(TREE_GATEWAY_FLAG_NOREPLY)?TagToExcludeClientFromReplies(calledBy,optOpTag):optOpTag);
}

status_t MuxTreeGateway :: TreeGateway_PingLocalPeer(ITreeGatewaySubscriber * calledBy, const String & tag, TreeGatewayFlags flags)
{
   return _isConnected ? ITreeGatewaySubscriber::PingTreeLocalPeer(PrependRegistrationIDPrefix(calledBy, tag), flags) : B_BAD_OBJECT;
}

status_t MuxTreeGateway :: TreeGateway_PingSeniorPeer(ITreeGatewaySubscriber * calledBy, const String & tag, uint32 whichDB, TreeGatewayFlags flags)
{
   return _isConnected ? ITreeGatewaySubscriber::PingTreeSeniorPeer(PrependRegistrationIDPrefix(calledBy, tag), whichDB, flags) : B_BAD_OBJECT;
}

status_t MuxTreeGateway :: TreeGateway_SendMessageToSeniorPeer(ITreeGatewaySubscriber * calledBy, const ConstMessageRef & msg, uint32 whichDB, const String & tag)
{
   return _isConnected ? ITreeGatewaySubscriber::SendMessageToTreeSeniorPeer(msg, whichDB, PrependRegistrationIDPrefix(calledBy, tag)) : B_BAD_OBJECT;
}

status_t MuxTreeGateway :: TreeGateway_SendMessageToSubscriber(ITreeGatewaySubscriber * calledBy, const String & subscriberPath, const ConstMessageRef & msg, const ConstQueryFilterRef & optFilterRef, const String & tag)
{
   return _isConnected ? ITreeGatewaySubscriber::SendMessageToSubscriber(subscriberPath, msg, optFilterRef, PrependRegistrationIDPrefix(calledBy, tag)) : B_BAD_OBJECT;
}

// Begin ITreeGatewaySubscriber callback API

void MuxTreeGateway :: TreeNodeUpdated(const String & path, const ConstMessageRef & nodeMsg, const String & optOpTag)
{
   if (optOpTag.StartsWith(_muxTreeGatewayIDPrefix))
   {
      String effectiveTag;
      ITreeGatewaySubscriber * dontNotifyMe = ParseRegistrationIDPrefix(optOpTag.Substring(_muxTreeGatewayIDPrefix.Length()), effectiveTag, '!');
      TreeNodeUpdatedAux(path, nodeMsg, effectiveTag, dontNotifyMe);
   }
   else if (optOpTag.StartsWith("[MUX"))
   {
      // If some other MuxTreeGateway specified the [MUX______] prefix, then we still want our subscribers to see this update, but without his prefix
      const int32 colonIdx = optOpTag.IndexOf(':');  // e.g. "[MUX1234535]!3!:userTag" -> "userTag"
      TreeNodeUpdatedAux(path, nodeMsg, (colonIdx>=0)?optOpTag.Substring(colonIdx+1):optOpTag, NULL);
   }
   else TreeNodeUpdatedAux(path, nodeMsg, optOpTag, NULL);
}

void MuxTreeGateway :: TreeNodeUpdatedAux(const String & path, const ConstMessageRef & msgRef, const String & optOpTag, ITreeGatewaySubscriber * optDontNotify)
{
   if (_allowedCallbacks.HasItems())
   {
      // In this case, it's faster to iterate over just what's allowed
      for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(_allowedCallbacks); iter.HasData(); iter++)
      {
         ITreeGatewaySubscriber * sub = iter.GetKey();
         TreeSubscriberInfo * subInfo = _subscriberInfos.GetWithDefault(sub)();
         if ((subInfo)&&(sub != optDontNotify)) UpdateSubscriber(sub, *subInfo, path, DoesPathMatch(sub, subInfo, path, msgRef()) ? msgRef : MessageRef(), optOpTag);
      }
   }
   else
   {
      for (HashtableIterator<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> iter(_subscriberInfos); iter.HasData(); iter++)
      {
         ITreeGatewaySubscriber * sub = iter.GetKey();
         TreeSubscriberInfo * subInfo = iter.GetValue()();
         if ((subInfo)&&(sub != optDontNotify)) UpdateSubscriber(sub, *subInfo, path, DoesPathMatch(sub, subInfo, path, msgRef()) ? msgRef : MessageRef(), optOpTag);
      }
   }
}

void MuxTreeGateway :: UpdateSubscriber(ITreeGatewaySubscriber * sub, TreeSubscriberInfo & subInfo, const String & path, const ConstMessageRef & msgRef, const String & optOpTag)
{
   if (msgRef())
   {
      EnsureSubscriberInBatchGroup(sub);
      (void) subInfo._receivedPaths.Put(path, path.GetNumInstancesOf('/')+1);
      sub->TreeNodeUpdated(path, msgRef, optOpTag);
   }
   else if (subInfo._receivedPaths.Remove(path).IsOK())
   {
      EnsureSubscriberInBatchGroup(sub);
      sub->TreeNodeUpdated(path, msgRef, optOpTag);
   }
}

void MuxTreeGateway :: TreeNodeIndexCleared(const String & path, const String & optOpTag)
{
   DoIndexNotifications(path, INDEX_OP_CLEARED, 0, GetEmptyString(), optOpTag);
}

void MuxTreeGateway :: TreeNodeIndexEntryInserted(const String & path, uint32 insertedAtIndex, const String & nodeName, const String & optOpTag)
{
   DoIndexNotifications(path, INDEX_OP_ENTRYINSERTED, insertedAtIndex, nodeName, optOpTag);
}

void MuxTreeGateway :: TreeNodeIndexEntryRemoved(const String & path, uint32 removedAtIndex, const String & nodeName, const String & optOpTag)
{
   DoIndexNotifications(path, INDEX_OP_ENTRYREMOVED, removedAtIndex, nodeName, optOpTag);
}

void MuxTreeGateway :: DoIndexNotifications(const String & path, char opCode, uint32 index, const String & nodeName, const String & optOpTag)
{
   if (_allowedCallbacks.HasItems())
   {
      // In this case, it's faster to iterate over just what's allowed
      for (HashtableIterator<ITreeGatewaySubscriber *, Void> iter(_allowedCallbacks); iter.HasData(); iter++)
      {
         ITreeGatewaySubscriber * sub = iter.GetKey();
         TreeSubscriberInfoRef * pmr = _subscriberInfos.Get(sub);
         if ((pmr)&&(DoesPathMatch(sub, pmr->GetItemPointer(), path, NULL))) DoIndexNotificationAux(sub, path, opCode, index, nodeName, optOpTag);
      }
   }
   else
   {
      for (HashtableIterator<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> iter(_subscriberInfos); iter.HasData(); iter++)
         if (DoesPathMatch(iter.GetKey(), iter.GetValue()(), path, NULL)) DoIndexNotificationAux(iter.GetKey(), path, opCode, index, nodeName, optOpTag);
   }
}

void MuxTreeGateway :: DoIndexNotificationAux(ITreeGatewaySubscriber * sub, const String & path, char opCode, uint32 index, const String & nodeName, const String & optOpTag)
{
   EnsureSubscriberInBatchGroup(sub);
   switch(opCode)
   {
      case INDEX_OP_CLEARED:       sub->TreeNodeIndexCleared(path, optOpTag);                        break;
      case INDEX_OP_ENTRYINSERTED: sub->TreeNodeIndexEntryInserted(path, index, nodeName, optOpTag); break;
      case INDEX_OP_ENTRYREMOVED:  sub->TreeNodeIndexEntryRemoved( path, index, nodeName, optOpTag); break;
   }
}

void MuxTreeGateway :: TreeLocalPeerPonged(const String & tag)
{
   if (tag.StartsWith("obss:"))
   {
      _allowedCallbacks.Clear();
      StringTokenizer tok(tag()+5, ",,");
      const char * t;
      while((t=tok()) != NULL)
      {
         ITreeGatewaySubscriber * s = ParseRegistrationID(t);
         (void) _allowedCallbacks.PutWithDefault(s?s:&_dummySubscriber);  // _dummySubscriber so that if the requester has since unregistered we still won't let callbacks spill out to everyone else
      }
   }
   else
   {
      String suffix;
      ITreeGatewaySubscriber * s = ParseRegistrationIDPrefix(tag, suffix);
      if (s) s->TreeLocalPeerPonged(suffix);
   }
}

void MuxTreeGateway :: TreeSeniorPeerPonged(const String & tag, uint32 whichDB)
{
   String suffix;
   ITreeGatewaySubscriber * s = ParseRegistrationIDPrefix(tag, suffix);
   if (s) s->TreeSeniorPeerPonged(suffix, whichDB);
}

void MuxTreeGateway :: MessageReceivedFromTreeSeniorPeer(int32 whichDB, const String & tag, const MessageRef & payload)
{
   String suffix;
   ITreeGatewaySubscriber * s = ParseRegistrationIDPrefix(tag, suffix);
   if (s) s->MessageReceivedFromTreeSeniorPeer(whichDB, suffix, payload);
}

// Returns true iff (path) is a string like "_4_:" or "_4_:_5_:" or "_4_:_5_:_6_:foo" or etc
static bool IsReturnAddress(const String & path)
{
   return ((path.StartsWith('_'))
        && (muscleInRange(path()[1], '0', '9'))
        && (path.StartsWith(String("_%1_:").Arg(atoi(path()+1)))));
}

void MuxTreeGateway :: MessageReceivedFromSubscriber(const String & nodePath, const MessageRef & payload, const String & returnAddress)
{
   const uint32 numPathSegments = nodePath.GetNumInstancesOf('/')+1;
   const SegmentedStringMatcher ssm(nodePath);

   if (IsReturnAddress(nodePath))
   {
      String suffix;
      ITreeGatewaySubscriber * sub = ParseRegistrationIDPrefix(nodePath, suffix);
      if (sub) sub->MessageReceivedFromSubscriber(suffix, payload, returnAddress);
   }
   else
   {
      // This might be inefficient in some cases -- perhaps there is a better way to do this?  -jaf
      Hashtable<ITreeGatewaySubscriber *, String> matchingSubscribers;  // subscriber -> matching-node-path
      for (HashtableIterator<ITreeGatewaySubscriber *, TreeSubscriberInfoRef> iter(_subscriberInfos); iter.HasData(); iter++)
      {
         const TreeSubscriberInfo * subInfo = iter.GetValue()();
         if (subInfo)
         {
            for (HashtableIterator<String, uint32> subIter(subInfo->_receivedPaths); subIter.HasData(); subIter++)
            {
               if ((subIter.GetValue() == numPathSegments)&&(ssm.Match(subIter.GetKey()())))
               {
                  (void) matchingSubscribers.Put(iter.GetKey(), subIter.GetKey());
                  break;
               }
            }
         }
      }
      for (HashtableIterator<ITreeGatewaySubscriber *, String> iter(matchingSubscribers); iter.HasData(); iter++) iter.GetKey()->MessageReceivedFromSubscriber(iter.GetValue(), payload, returnAddress);
   }
}

void MuxTreeGateway :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   bool isResponseToRequestNodeValues = false;
   String suffix;
   ITreeGatewaySubscriber * subPtr = static_cast<ITreeGatewaySubscriber *>(ParseRegistrationIDPrefix(tag, suffix));
   Queue<String> * q = _requestedSubtrees.Get(subPtr);
   if (q == NULL)
   {
      subPtr = static_cast<ITreeGatewaySubscriber *>(ParseRegistrationIDPrefix(tag, suffix, '='));  // special markerChar '=' means this is the response to a RequestNodeValues() call
      q = _requestedSubtrees.Get(subPtr);
      isResponseToRequestNodeValues = true;
   }

   if ((q)&&(q->RemoveFirstInstanceOf(suffix).IsOK()))
   {
      if (isResponseToRequestNodeValues)
      {
         if (subtreeData())
         {
            EnsureSubscriberInBatchGroup(subPtr);
            for (MessageFieldNameIterator fnIter(*subtreeData(), B_MESSAGE_TYPE); fnIter.HasData(); fnIter++)
            {
               const String & path = fnIter.GetFieldName();

               ConstMessageRef nodeMsg;
               MessageRef payloadMsg;
               if ((subtreeData()->FindMessage(path, nodeMsg).IsOK())&&(nodeMsg()->FindMessage(PR_NAME_NODEDATA, payloadMsg).IsOK())) subPtr->TreeNodeUpdated(path, payloadMsg, suffix);
            }
         }
      }
      else subPtr->SubtreesRequestResultReturned(suffix, subtreeData);

      if (q->IsEmpty()) (void) _requestedSubtrees.Remove(subPtr);
   }
}

status_t MuxTreeGateway :: UpdateSubscription(const String & subscriptionPath, ITreeGatewaySubscriber * optSubscriber, TreeGatewayFlags flags)
{
   const Queue<SubscriptionInfo> * filterQueue = _subscribedStrings.Get(subscriptionPath);
   if ((filterQueue)&&(filterQueue->HasItems()))
   {
      const Queue<SubscriptionInfo> & q = *filterQueue;
      String obss; if (optSubscriber) obss = GetRegistrationIDPrefix(optSubscriber);

      ConstQueryFilterRef sendFilter;  // what we will actually send
      ConstQueryFilterRef unionFilter; // the "matches against any of the following" meta-filter, demand-allocated
      bool useFilter = true;
      for (int32 i=q.GetLastValidIndex(); i>=0; i--)
      {
         const SubscriptionInfo & nextItem = q[i];
         if (optSubscriber == NULL)
         {
            if (obss.HasChars()) obss += ',';
            obss += GetRegistrationIDPrefix(nextItem.GetSubscriber());
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
                     oqf = new OrQueryFilter;
                     if (oqf)
                     {
                        unionFilter.SetRef(oqf);
                        (void) oqf->GetChildren().AddTail(sendFilter);
                        sendFilter = unionFilter;
                     }
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

      if (obss.HasChars()) MRETURN_ON_ERROR(ITreeGatewaySubscriber::PingTreeLocalPeer(obss.WithPrepend("obss:"))); // mark the beginning of our returned results
      MRETURN_ON_ERROR(ITreeGatewaySubscriber::AddTreeSubscription(subscriptionPath, sendFilter, flags));
      if (obss.HasChars()) MRETURN_ON_ERROR(ITreeGatewaySubscriber::PingTreeLocalPeer("obss:"));               // mark the end of our returned results
      return B_NO_ERROR;
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
         (void) ITreeGatewaySubscriber::RemoveAllTreeSubscriptions();
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
   if ((pm)&&((_allowedCallbacks.IsEmpty())||(_allowedCallbacks.ContainsKey(sub))))
   {
      _dummyNode.SetNodeName(path.Substring("/"));  // in case (pm) is using a NodeNameQueryFilter
      return pm->MatchesPath(path(), optMessage, &_dummyNode);
   }
   else return false;
}

void MuxTreeGateway :: EnsureSubscriberInBatchGroup(ITreeGatewaySubscriber * sub)
{
   if ((IGatewaySubscriber::IsInCallbackBatch())&&(_needsCallbackBatchEndsCall.ContainsKey(sub) == false))  // jaf
   {
      (void) _needsCallbackBatchEndsCall.PutWithDefault(sub);
      CallBeginCallbackBatch(sub);
   }
}

void MuxTreeGateway :: CallbackBatchBegins()
{
   // deliberately empty; we'll tell our subscribers about the batch just before we call their one of their callback-methods for the first time
}

void MuxTreeGateway :: CallbackBatchEnds()
{
   // Notify only the subscribers that we previously called BeginCallbackBatch() on
   while(_needsCallbackBatchEndsCall.HasItems())
   {
      ITreeGatewaySubscriber * a = *_needsCallbackBatchEndsCall.GetFirstKey();
      CallEndCallbackBatch(a);
      (void) _needsCallbackBatchEndsCall.RemoveFirst();
   }
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
      if (_allowedCallbacks.Remove(sub).IsOK()) (void) _allowedCallbacks.PutWithDefault(&_dummySubscriber);  // _dummySubscriber so that if the requester has since unregistered we still won't let callbacks spill out to everyone else
      (void) _needsCallbackBatchEndsCall.Remove(sub);
      (void) _subscriberInfos.Remove(sub);
      (void) _requestedSubtrees.Remove(sub);
      (void) TreeGateway_RemoveAllSubscriptions(sub, TreeGatewayFlags());
   }
   ProxyTreeGateway::UnregisterSubscriber(s);
}

};

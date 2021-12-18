#ifndef ServerSideMessageUtilityFunctions_h
#define ServerSideMessageUtilityFunctions_h

#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"  // for TreeGatewayFlags

namespace zg {

/** Creates a Message that can be passed to a MUSCLE server to request a new node-subscription.
  * @param subscriptionPath a session-relative session-path (wildcards okay) to subscribe to
  * @param optFilterRef an optional QueryFilter object to limit the subscription based on the contents of the matching nodes' data-payloads
  * @param flags set of TREE_GATEWAY_FLAG_* values to adjust the behavior of the subscription
  * @param retMsg on success, the created Message is written into this object.
  * @returns B_NO_ERROR on success, or some other error code on failure.
  */
status_t CreateMuscleSubscribeMessage(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef, TreeGatewayFlags flags, MessageRef & retMsg);

/** Creates a Message that can be passed to a MUSCLE server to cancel an existing node-subscription.
  * @param subscriptionPath the session-relative node-path to cancel from (as was previously passed to CreateMuscleSubscribeMessage())
  * @param retMsg on success, the created Message is written into this object.
  * @returns B_NO_ERROR on success, or some other error code on failure.
  */
status_t CreateMuscleUnsubscribeMessage(const String & subscriptionPath, MessageRef & retMsg);

/** Creates a Message that can be passed to a MUSCLE server to cancel all existing node-subscriptions.
  * @param retMsg on success, the created Message is written into this object.
  * @returns B_NO_ERROR on success, or some other error code on failure.
  */
status_t CreateMuscleUnsubscribeAllMessage(MessageRef & retMsg);

/** Creates a Message that can be passed to a MUSCLE server to request a one-time (re)send of matching nodes
  * @param nodePath the session-relative node-path to use to select nodes to send
  * @param optFilterRef an optional QueryFilter object to limit the matching, based on the contents of the matching nodes' data-payloads
  * @param retMsg on success, the created Message is written into this object.
  * @param tag an arbitrary tag-string to use to identify the results.
  * @returns B_NO_ERROR on success, or some other error code on failure.
  */
status_t CreateMuscleRequestNodeValuesMessage(const String & nodePath, const ConstQueryFilterRef & optFilterRef, MessageRef & retMsg, const String & tag);

/** Creates a Message that can be passed to a MUSCLE server to request a one-time sending of one or more subtrees of data
  * @param queryStrings a list of session-relative node-paths (wildcards are okay) to use to select nodes to send
  * @param queryFilters an optional list of QueryFilter objects to limit the matching on each QueryString, based on the contents of the matching nodes' data-payloads
  * @param tag an arbitrary tag-string to use to identify the results
  * @param maxDepth the maximum depth that the recursion should descent to (pass MUSCLE_NO_LIMIT if you don't want to specify a maximum depth)
  * @param retMsg on success, the created Message is written into this object.
  * @returns B_NO_ERROR on success, or some other error code on failure.
  */
status_t CreateMuscleRequestNodeSubtreesMessage(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, MessageRef & retMsg);

};  // end namespace zg

#endif

#include "zg/gateway/tree/ITreeGateway.h"

namespace zg {

void ITreeGateway :: ShutdownGateway()
{
   for (HashtableIterator<IGatewaySubscriber *, Void> iter(GetRegisteredSubscribers()); iter.HasData(); iter++) static_cast<ITreeGatewaySubscriber *>(iter.GetKey())->TreeGatewayShuttingDown();
   IGateway::ShutdownGateway();
}

};


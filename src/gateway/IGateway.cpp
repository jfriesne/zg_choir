#include "zg/gateway/IGateway.h"

namespace zg {

void IGatewaySubscriber :: SetGateway(IGateway * optGateway)
{
   if (optGateway != _gateway)
   {
      if (_gateway) _gateway->UnregisterSubscriber(this);
      _gateway = optGateway;
      if (_gateway) _gateway->RegisterSubscriber(this);
   }
}

};


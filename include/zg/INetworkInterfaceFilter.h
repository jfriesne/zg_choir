#ifndef INetworkInterfaceFilter_h
#define INetworkInterfaceFilter_h

#include "zg/ZGNameSpace.h"
#include "util/NetworkInterfaceInfo.h"

namespace zg
{

/** Interface for an object that can decide whether or not ZG should use
  * the specified network interface.
  */
class INetworkInterfaceFilter
{
public:
   INetworkInterfaceFilter() {/* empty */}
   virtual ~INetworkInterfaceFilter() {/* empty */}

   /** Should be implemented to true iff the Network Interface described
     * by the argument should be used by ZGChoir.
     * @param nii a NetworkInterfaceInfo object (as previously returned by GetNetworkInterfaceInfos())
     */
   MUSCLE_NODISCARD virtual bool IsOkayToUseNetworkInterface(const NetworkInterfaceInfo & nii) const = 0;
};

};  // end namespace zg

#endif

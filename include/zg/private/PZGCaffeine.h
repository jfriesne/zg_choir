#ifndef PZGCaffeine_h
#define PZGCaffeine_h

#include "zg/private/PZGNameSpace.h"

namespace zg_private
{

/** Under MacOS/X, the constructor of this object will call NSProcessInfo's beginActivityWithOptions
  * method in order to prevent MacOS/X from applying app-nap to this process for the specified reason.
  * This object's destructor will call NSProcessInfo's endActivityWithOptions to allow app-nap to
  * occur again (if no other PZGCaffeine objects still exist in the process)
  * Under other OS's, this class is currently a no-op.
  */
class PZGCaffeine
{
public:
   /** Constructor -- calls beginActivityWithOptions() when executing under MacOS/X, otherwise does nothing.
     * @param reason the human-readable explanation for why we are not allowed to nap.  If passed as NULL, then this class will be a no-op.
     */
   PZGCaffeine(const char * reason);

   /** Destructor -- calls endActivityWithOptions() when executing under MacOS/X, otherwise does nothing.  */
   ~PZGCaffeine();

private:
   void * _activityID;
};

};  // end namespace zg_private

#endif // PZGCaffeine_h

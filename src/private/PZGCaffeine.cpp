#include "zg/private/PZGCaffeine.h"

#ifdef __APPLE__
extern void * begin_disable_app_nap(const char * reason);
extern void     end_disable_app_nap(void * activityID);
#endif

namespace zg_private
{

PZGCaffeine :: PZGCaffeine(const char * reason) : _activityID(NULL)
{
#ifdef __APPLE__
   if (reason)
   {
      _activityID = begin_disable_app_nap(reason);
      if (_activityID == NULL) LogTime(MUSCLE_LOG_CRITICALERROR, "begin_disable_app_nap(%s) failed!\n", reason);
   }
#else
   (void) reason;
#endif
}

PZGCaffeine :: ~PZGCaffeine()
{
#ifdef __APPLE__
   if (_activityID) end_disable_app_nap(_activityID);
#endif
}

};  // end namespace zg_private

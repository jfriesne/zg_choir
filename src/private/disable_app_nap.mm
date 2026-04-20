#ifdef __APPLE__
# import <Foundation/Foundation.h>
# import <Foundation/NSProcessInfo.h>

void * begin_disable_app_nap(const char * reasonArg);  // just to avoid a -Wmissing-prototypes warning
void * begin_disable_app_nap(const char * reasonArg)
{
   if ([[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)])
   {
      return (__bridge void *) [[NSProcessInfo processInfo] beginActivityWithOptions:0x00FFFFFF reason:[NSString stringWithUTF8String:reasonArg]];
   }
   return NULL;
}

void end_disable_app_nap(void * idPtr);  // just to avoid a -Wmissing-prototypes warning
void end_disable_app_nap(void * idPtr)
{
   if (idPtr)
   {
      [[NSProcessInfo processInfo] endActivity:(__bridge id) idPtr];
   }
}

#endif

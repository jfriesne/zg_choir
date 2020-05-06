#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

void disable_app_nap(void) 
{
   if ([[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)]) 
   {
      [[NSProcessInfo processInfo] beginActivityWithOptions:0x00FFFFFF reason:@"Sending heartbeats"];
   }
}

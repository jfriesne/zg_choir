#if TARGET_OS_OSX

#import <AppKit/NSAppearance.h>
#import <AppKit/NSApplication.h>

void disable_dark_mode();  // avoid compiler warning
void disable_dark_mode()
{
   NSAppearance * lightAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
   [NSApp setAppearance:lightAppearance];
}

bool is_in_dark_mode()
{
    auto appearance = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames: @[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [appearance isEqualToString:NSAppearanceNameDarkAqua];
}

#endif  // TARGET_OS_OSX

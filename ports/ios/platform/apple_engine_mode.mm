// Which engine the launcher loads next time. Singleplayer and multiplayer are separate dylibs
// (see ports/ios/app/launcher.mm), so switching is a restart, not a hand-off: the running engine
// already owns Metal, the audio device and its hunk, and cannot be torn down from inside itself.
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include "apple_engine_mode.h"

static NSString *const kModeKey = @"KisakEngineMode";

void KisakApple_SetEngineMode(const char *mode)
{
    if (!mode)
        return;
    @autoreleasepool {
        [NSUserDefaults.standardUserDefaults setObject:[NSString stringWithUTF8String:mode] forKey:kModeKey];
        [NSUserDefaults.standardUserDefaults synchronize];
    }
}

const char *KisakApple_GetEngineMode()
{
    @autoreleasepool {
        NSString *mode = [NSUserDefaults.standardUserDefaults stringForKey:kModeKey];
        return [mode isEqualToString:@"sp"] ? "sp" : "mp";
    }
}

void KisakApple_PromptEngineRestart(const char *mode)
{
    @autoreleasepool {
        NSString *target = [NSString stringWithUTF8String:mode ? mode : "mp"];
        NSString *name = [target isEqualToString:@"sp"] ? @"Singleplayer" : @"Multiplayer";
        dispatch_async(dispatch_get_main_queue(), ^{
            UIWindow *window = nil;
            for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
                if ([scene isKindOfClass:UIWindowScene.class]) {
                    for (UIWindow *candidate in ((UIWindowScene *)scene).windows) {
                        if (candidate.isKeyWindow) { window = candidate; break; }
                    }
                    if (!window) window = ((UIWindowScene *)scene).windows.firstObject;
                }
                if (window) break;
            }
            if (!window.rootViewController)
                return;
            UIAlertController *alert = [UIAlertController
                alertControllerWithTitle:[NSString stringWithFormat:@"Switching to %@", name]
                                 message:@"Close the app and open it again to finish switching."
                          preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
            [window.rootViewController presentViewController:alert animated:YES completion:nil];
        });
    }
}

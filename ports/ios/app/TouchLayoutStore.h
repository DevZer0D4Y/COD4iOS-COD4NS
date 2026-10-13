#pragma once
#import <Foundation/Foundation.h>

// One file in the application container: both engine dylibs use this store.
@interface KISTouchLayoutStore : NSObject
+ (NSURL *)defaultURL;
+ (NSDictionary *)loadAtURL:(NSURL *)url defaults:(NSUserDefaults *)defaults;
+ (BOOL)saveLayout:(NSArray *)layout opacity:(float)opacity atURL:(NSURL *)url
         defaults:(NSUserDefaults *)defaults error:(NSError **)error;
@end

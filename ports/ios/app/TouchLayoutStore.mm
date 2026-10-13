#import "TouchLayoutStore.h"
#include <cmath>

@implementation KISTouchLayoutStore
+ (NSURL *)defaultURL {
    NSURL *directory=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                        inDomains:NSUserDomainMask].firstObject;
    return [directory URLByAppendingPathComponent:@"cod4.touch.layout.v2.plist"];
}
+ (BOOL)validLayout:(id)layout {
    if(![layout isKindOfClass:NSArray.class] || [layout count]!=13)return NO;
    for(id entry in layout){
        if(![entry isKindOfClass:NSArray.class] || [entry count]!=3)return NO;
        for(id number in entry)
            if(![number isKindOfClass:NSNumber.class] || !std::isfinite([number doubleValue]))return NO;
    }
    return YES;
}
+ (NSDictionary *)loadAtURL:(NSURL *)url defaults:(NSUserDefaults *)defaults {
    NSDictionary *saved=[NSDictionary dictionaryWithContentsOfURL:url];
    if([self validLayout:saved[@"layout"]] && [saved[@"opacity"] isKindOfClass:NSNumber.class]
        && std::isfinite([saved[@"opacity"] doubleValue]))return saved;
    NSArray *legacy=[defaults arrayForKey:@"cod4.touch.layout.v1"];
    if(![self validLayout:legacy])return nil;
    NSNumber *opacity=[defaults objectForKey:@"cod4.touch.opacity.v1"];
    if(![opacity isKindOfClass:NSNumber.class] || !std::isfinite(opacity.doubleValue))opacity=@1;
    return @{@"layout":legacy,@"opacity":opacity};
}
+ (BOOL)saveLayout:(NSArray *)layout opacity:(float)opacity atURL:(NSURL *)url
         defaults:(NSUserDefaults *)defaults error:(NSError **)error {
    if(![self validLayout:layout] || !std::isfinite(opacity)){
        if(error)*error=[NSError errorWithDomain:@"COD4TouchLayout" code:1
            userInfo:@{NSLocalizedDescriptionKey:@"Invalid touch layout"}];
        return NO;
    }
    NSDictionary *saved=@{@"layout":layout,@"opacity":@(opacity)};
    NSData *data=[NSPropertyListSerialization dataWithPropertyList:saved
        format:NSPropertyListBinaryFormat_v1_0 options:0 error:error];
    if(!data || ![NSFileManager.defaultManager createDirectoryAtURL:url.URLByDeletingLastPathComponent
        withIntermediateDirectories:YES attributes:nil error:error]
        || ![data writeToURL:url options:NSDataWritingAtomic error:error])return NO;
    // Keep the previous preference keys compatible with older app versions.
    [defaults setObject:layout forKey:@"cod4.touch.layout.v1"];
    [defaults setFloat:opacity forKey:@"cod4.touch.opacity.v1"];
    return YES;
}
@end

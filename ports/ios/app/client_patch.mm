#import "client_patch.h"
#import <CommonCrypto/CommonDigest.h>

namespace {
constexpr NSUInteger patchSize=550611;
NSString *const patchSHA=@"02a834ca1c1278ec9bd2113f402850454f5c39aa787603dc5132cbb114fda01a";
NSString *const patchURL=@"https://raw.githubusercontent.com/callofduty4x/CoD4x_Client_pub/dbb2eccfd0f6914d580aacae6d6193eb7060891e/assets/cod4x/zone/cod4x_patchv2.ff";
NSError *failure(NSString *message) {
    return [NSError errorWithDomain:@"COD4iOS.ClientPatch" code:1
        userInfo:@{NSLocalizedDescriptionKey:message}];
}
NSArray<NSString *> *destinations(NSString *documents) {
    NSFileManager *files=NSFileManager.defaultManager;
    NSString *zone=[documents stringByAppendingPathComponent:@"zone"];
    NSSet *languages=[NSSet setWithArray:@[@"english",@"french",@"german",@"italian",@"spanish",
        @"british",@"russian",@"polish",@"korean",@"taiwanese",@"japanese",@"chinese",@"czech"]];
    NSMutableArray *paths=[NSMutableArray array];
    for(NSString *name in [files contentsOfDirectoryAtPath:zone error:nil]) {
        if(![languages containsObject:name.lowercaseString])continue;
        NSString *folder=[zone stringByAppendingPathComponent:name];
        NSDictionary *info=[files attributesOfItemAtPath:folder error:nil];
        if(![info[NSFileType] isEqual:NSFileTypeDirectory])continue;
        [paths addObject:[folder stringByAppendingPathComponent:@"cod4x_patchv2.ff"]];
    }
    return paths;
}
NSError *install(NSData *data,NSArray<NSString *> *paths) {
    if(!KisakCoD4xPatchValid(data))return failure(@"CoD4x client patch verification failed.");
    NSFileManager *files=NSFileManager.defaultManager;
    for(NSString *path in paths) {
        NSData *current=[NSData dataWithContentsOfFile:path];
        if(KisakCoD4xPatchValid(current))continue;
        NSError *error=nil;
        if(current) {
            NSString *backup=[path stringByAppendingString:@".before-cod4ios-client-patch"];
            if(![files fileExistsAtPath:backup] && ![files copyItemAtPath:path toPath:backup error:&error])return error;
        }
        if(![data writeToFile:path options:NSDataWritingAtomic error:&error])return error;
        if(!KisakCoD4xPatchValid([NSData dataWithContentsOfFile:path]))return failure(@"CoD4x client patch could not be saved.");
    }
    return nil;
}
}
BOOL KisakCoD4xPatchValid(NSData *data) {
    if(data.length!=patchSize)return NO;
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(data.bytes,(CC_LONG)data.length,digest);
    NSMutableString *hex=[NSMutableString stringWithCapacity:64];
    for(unsigned char value:digest)[hex appendFormat:@"%02x",value];
    return [hex isEqual:patchSHA];
}
void KisakPrepareCoD4xPatch(NSString *documents,void (^completion)(NSError *)) {
    // Disk work and network completion never block UIKit input/render callbacks.
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        NSArray *paths=destinations(documents);
        auto finish=^(NSError *error){dispatch_async(dispatch_get_main_queue(),^{completion(error);});};
        if(!paths.count){finish(failure(@"No supported game language folder found in zone/."));return;}
        NSData *valid=nil;BOOL allValid=YES;
        for(NSString *path in paths) {
            NSData *data=[NSData dataWithContentsOfFile:path];
            if(KisakCoD4xPatchValid(data))valid=data;else allValid=NO;
        }
        if(allValid){finish(nil);return;}
        if(valid){finish(install(valid,paths));return;}
        NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:[NSURL URLWithString:patchURL]];
        request.timeoutInterval=30;
        request.cachePolicy=NSURLRequestReloadIgnoringLocalCacheData;
        NSURLSessionConfiguration *configuration=NSURLSessionConfiguration.ephemeralSessionConfiguration;
        configuration.timeoutIntervalForResource=45;
        NSURLSession *session=[NSURLSession sessionWithConfiguration:configuration];
        NSURLSessionDataTask *task=[session dataTaskWithRequest:request completionHandler:^(NSData *data,NSURLResponse *response,NSError *error){
            if(!error && (![response isKindOfClass:NSHTTPURLResponse.class] || ((NSHTTPURLResponse *)response).statusCode!=200))
                error=failure(@"The official CoD4x patch download failed. Check your connection and reopen the app.");
            if(!error)error=install(data,paths);
            [session finishTasksAndInvalidate];finish(error);
        }];
        [task resume];
    });
}

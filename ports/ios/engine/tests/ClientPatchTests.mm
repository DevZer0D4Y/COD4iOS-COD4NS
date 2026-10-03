// Host regression test. Pass the official cod4x_patchv2.ff as argv[1].
// Add --network to verify a fresh installation's HTTPS bootstrap as well.
#import "../../app/client_patch.mm"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
    @autoreleasepool {
        assert(argc >= 2);
        NSData *official=[NSData dataWithContentsOfFile:@(argv[1])];
        assert(KisakCoD4xPatchValid(official));
        assert(!KisakCoD4xPatchValid(nil));
        NSMutableData *corrupt=[official mutableCopy];
        ((unsigned char *)corrupt.mutableBytes)[100]^=1;
        assert(!KisakCoD4xPatchValid(corrupt));
        NSFileManager *files=NSFileManager.defaultManager;
        NSString *root=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        NSString *english=[root stringByAppendingPathComponent:@"zone/english"];
        NSString *italian=[root stringByAppendingPathComponent:@"zone/italian"];
        assert([files createDirectoryAtPath:english withIntermediateDirectories:YES attributes:nil error:nil]);
        assert([files createDirectoryAtPath:italian withIntermediateDirectories:YES attributes:nil error:nil]);
        NSArray *paths=destinations(root);assert(paths.count==2);
        NSString *en=[english stringByAppendingPathComponent:@"cod4x_patchv2.ff"];
        NSString *it=[italian stringByAppendingPathComponent:@"cod4x_patchv2.ff"];
        assert([corrupt writeToFile:en atomically:YES]);
        assert(install(corrupt,paths)); // Never replaces files with unverified bytes.
        assert([[NSData dataWithContentsOfFile:en] isEqual:corrupt]);
        assert(![files fileExistsAtPath:it]);
        assert(!install(official,paths));
        assert(KisakCoD4xPatchValid([NSData dataWithContentsOfFile:en]));
        assert(KisakCoD4xPatchValid([NSData dataWithContentsOfFile:it]));
        NSString *backup=[en stringByAppendingString:@".before-cod4ios-client-patch"];
        assert([[NSData dataWithContentsOfFile:backup] isEqual:corrupt]);
        assert(!install(official,paths)); // Idempotent; original backup is retained.
        assert([[NSData dataWithContentsOfFile:backup] isEqual:corrupt]);
        assert([files removeItemAtPath:it error:nil]);
        // Recover a missing language copy from the valid local copy without network.
        KisakPrepareCoD4xPatch(root,^(NSError *error){
            assert(NSThread.isMainThread);assert(!error);
            assert(KisakCoD4xPatchValid([NSData dataWithContentsOfFile:it]));
            KisakPrepareCoD4xPatch(root,^(NSError *second){
                assert(!second);assert(NSThread.isMainThread);
                if(argc<3 || strcmp(argv[2],"--network")) {
                    [files removeItemAtPath:root error:nil];puts("Client patch offline tests passed");exit(0);
                }
                assert([files removeItemAtPath:en error:nil]);
                assert([files removeItemAtPath:it error:nil]);
                KisakPrepareCoD4xPatch(root,^(NSError *download){
                    if(download){fprintf(stderr,"%s\n",download.localizedDescription.UTF8String);exit(1);}
                    assert(NSThread.isMainThread);
                    assert(KisakCoD4xPatchValid([NSData dataWithContentsOfFile:en]));
                    assert(KisakCoD4xPatchValid([NSData dataWithContentsOfFile:it]));
                    [files removeItemAtPath:root error:nil];
                    puts("Client patch offline and fresh-install HTTPS tests passed");exit(0);
                });
            });
        });
    }
    CFRunLoopRun();
    return 1;
}

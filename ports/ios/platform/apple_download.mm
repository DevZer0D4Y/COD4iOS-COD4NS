// HTTP(S) game-asset downloads. Each request owns its completion state so a cancelled
// transfer cannot finish, overwrite a file, or report failure on the following request.
#import <Foundation/Foundation.h>
#include <memory>
#include <mutex>
#include "apple_download.h"

namespace {
struct Transfer {
    std::mutex mutex;
    NSURLSessionDownloadTask *task = nil;
    NSURLSession *session = nil;
    int status = 0;
    bool cancelled = false;
    bool motd = false;
};
// Accessed only by the engine thread. Completion handlers retain their own Transfer.
std::shared_ptr<Transfer> current;
bool lastMotd = false;
}
int KisakDownload_Begin(const char *localName, const char *remoteName, int isMotd) {
    if (!localName || !*localName || !remoteName || !*remoteName) return 0;
    KisakDownload_Cancel();
    @autoreleasepool {
        NSString *urlText = [NSString stringWithUTF8String:remoteName];
        NSURL *url = [NSURL URLWithString:urlText];
        if (!url || !([url.scheme.lowercaseString isEqualToString:@"http"] || [url.scheme.lowercaseString isEqualToString:@"https"])) return 0;
        NSString *destination = [NSString stringWithUTF8String:localName];
        if (!destination) return 0;
        auto transfer = std::make_shared<Transfer>();
        transfer->motd = isMotd != 0; lastMotd = transfer->motd;
        NSURLSessionConfiguration *configuration = NSURLSessionConfiguration.ephemeralSessionConfiguration;
        configuration.timeoutIntervalForRequest = 30;
        configuration.HTTPAdditionalHeaders = @{@"User-Agent": @"CoD4 MP"};
        transfer->session = [NSURLSession sessionWithConfiguration:configuration];
        transfer->task = [transfer->session downloadTaskWithURL:url completionHandler:^(NSURL *location, NSURLResponse *response, NSError *error) {
            std::lock_guard<std::mutex> lock(transfer->mutex);
            if (transfer->cancelled) return;
            if (error || !location) {
                fprintf(stderr, "Download failed: %s\n", error ? error.localizedDescription.UTF8String : "no data");
                transfer->status = 2; return;
            }
            if (![response isKindOfClass:NSHTTPURLResponse.class] || ((NSHTTPURLResponse *)response).statusCode < 200 || ((NSHTTPURLResponse *)response).statusCode >= 300) {
                fprintf(stderr, "Download failed: HTTP response rejected\n"); transfer->status = 2; return;
            }
            NSFileManager *files = NSFileManager.defaultManager;
            NSError *saveError = nil;
            if (![files createDirectoryAtPath:destination.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:&saveError]) {
                fprintf(stderr, "Download directory: %s\n", saveError.localizedDescription.UTF8String);
                transfer->status = 2; return;
            }
            [files removeItemAtPath:destination error:nil];
            const BOOL moved = [files moveItemAtPath:location.path toPath:destination error:&saveError];
            if (!moved) fprintf(stderr, "Download save failed: %s\n", saveError.localizedDescription.UTF8String);
            else fprintf(stderr, "Download saved; awaiting game checksum verification\n");
            transfer->status = moved ? 1 : 2;
        }];
        current = transfer;
        [transfer->task resume];
        // Invalidate after this task finishes, releasing the session and its completion block.
        [transfer->session finishTasksAndInvalidate];
    }
    return 1;
}
int KisakDownload_Poll() {
    auto transfer = current;
    if (!transfer) return 2;
    std::lock_guard<std::mutex> lock(transfer->mutex);
    const int state = transfer->status;
    if (state) current.reset();
    return state;
}
void KisakDownload_Cancel() {
    auto transfer = current;
    current.reset();
    if (!transfer) return;
    {
        std::lock_guard<std::mutex> lock(transfer->mutex);
        transfer->cancelled = true;
    }
    [transfer->session invalidateAndCancel];
}
int KisakDownload_InProgress() { return current ? 1 : 0; }
int KisakDownload_IsMotd() { return lastMotd ? 1 : 0; }
void KisakDownload_Progress(long long *received, long long *expected) {
    auto transfer = current;
    if (received) *received = transfer ? transfer->task.countOfBytesReceived : 0;
    if (expected) *expected = transfer ? transfer->task.countOfBytesExpectedToReceive : 0;
}

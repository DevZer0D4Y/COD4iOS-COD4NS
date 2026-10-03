// AVFoundation implementation of the small video API in video_player_apple.h.
//
// AVPlayer drives both video and audio: frames are pulled as planar 4:2:0 pixel buffers through
// an AVPlayerItemVideoOutput, while the soundtrack goes straight to the system mixer (the game's
// own OpenAL mixer never sees it, which matches how Bink played movie audio through Miles).

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>

#include "video_player_apple.h"

struct KisakVideoPlayer
{
    AVPlayer *player;
    AVPlayerItemVideoOutput *output;
    CVPixelBufferRef frame;
    bool started;        // at least one frame was decoded
    double openHostTime; // CACurrentMediaTime when playback started
    double lastFrameTime;
};

KisakVideoPlayer *KisakVideo_Open(const char *path, float volume)
{
    if (!path || !*path)
        return nullptr;
    @autoreleasepool {
        NSString *file = [NSString stringWithUTF8String:path];
        if (![NSFileManager.defaultManager fileExistsAtPath:file])
            return nullptr;
        AVPlayerItem *item = [AVPlayerItem playerItemWithURL:[NSURL fileURLWithPath:file]];
        if (!item)
            return nullptr;
        NSDictionary *attributes = @{
            (NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8PlanarFullRange),
        };
        KisakVideoPlayer *handle = new KisakVideoPlayer();
        handle->output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attributes];
        [item addOutput:handle->output];
        handle->player = [AVPlayer playerWithPlayerItem:item];
        handle->player.volume = volume;
        handle->player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
        handle->frame = nullptr;
        handle->started = false;
        handle->openHostTime = CACurrentMediaTime();
        handle->lastFrameTime = handle->openHostTime;
        [handle->player play];
        // ARC manages the Objective-C members of this C++ struct (it emits the retain/release
        // for them), so no manual CFRetain here - doing both over-released the player.
        return handle;
    }
}

void KisakVideo_Close(KisakVideoPlayer *handle)
{
    if (!handle)
        return;
    @autoreleasepool {
        [handle->player pause];
        if (handle->frame)
        {
            CVPixelBufferUnlockBaseAddress(handle->frame, kCVPixelBufferLock_ReadOnly);
            CVPixelBufferRelease(handle->frame);
            handle->frame = nullptr;
        }
        handle->player = nil;
        handle->output = nil;
    }
    delete handle;
}

int KisakVideo_NextFrame(KisakVideoPlayer *handle, KisakVideoFrame *frame)
{
    if (!handle || !frame)
        return 0;
    @autoreleasepool {
        const CMTime itemTime = [handle->output itemTimeForHostTime:CACurrentMediaTime()];
        if (![handle->output hasNewPixelBufferForItemTime:itemTime])
            return 0;
        CVPixelBufferRef buffer = [handle->output copyPixelBufferForItemTime:itemTime itemTimeForDisplay:nullptr];
        if (!buffer)
            return 0;
        if (handle->frame)
        {
            CVPixelBufferUnlockBaseAddress(handle->frame, kCVPixelBufferLock_ReadOnly);
            CVPixelBufferRelease(handle->frame);
        }
        handle->frame = buffer;
        handle->started = true;
        handle->lastFrameTime = CACurrentMediaTime();
        CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        for (int plane = 0; plane < 3; ++plane)
        {
            frame->planes[plane] = static_cast<const uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(buffer, plane));
            frame->strides[plane] = CVPixelBufferGetBytesPerRowOfPlane(buffer, plane);
            frame->widths[plane] = static_cast<int>(CVPixelBufferGetWidthOfPlane(buffer, plane));
            frame->heights[plane] = static_cast<int>(CVPixelBufferGetHeightOfPlane(buffer, plane));
        }
        return 1;
    }
}

// A movie that never ends leaves the engine drawing a black full-screen quad over the game, so
// this errs towards "finished": besides the normal end of playback it also reports done when the
// item fails, when playback stalls, and when no frame ever arrives.
int KisakVideo_AtEnd(const KisakVideoPlayer *handle)
{
    if (!handle)
        return 1;
    AVPlayerItem *item = handle->player.currentItem;
    if (!item || item.status == AVPlayerItemStatusFailed)
        return 1;

    const double now = CACurrentMediaTime();
    const double elapsed = now - handle->openHostTime;
    const CMTime duration = item.duration;
    const double durationSeconds = CMTIME_IS_NUMERIC(duration) ? CMTimeGetSeconds(duration) : -1.0;

    if (durationSeconds > 0.0)
    {
        const double position = CMTIME_IS_NUMERIC(item.currentTime) ? CMTimeGetSeconds(item.currentTime) : 0.0;
        if (position >= durationSeconds - 0.05)
            return 1;
        // Playback that stopped making progress (a stall, or a pause at the end that the item
        // time never reflects) still has to end the movie.
        if (elapsed > durationSeconds + 2.0)
            return 1;
    }
    else if (elapsed > 60.0)
    {
        return 1;
    }

    if (!handle->started)
        return elapsed > 5.0 ? 1 : 0; // never decoded a frame: treat as unplayable
    if (handle->player.rate == 0.0f && now - handle->lastFrameTime > 0.5)
        return 1;
    return now - handle->lastFrameTime > 2.0 ? 1 : 0;
}

uint32_t KisakVideo_TimeMsec(const KisakVideoPlayer *handle)
{
    if (!handle)
        return 0;
    AVPlayerItem *item = handle->player.currentItem;
    if (!item)
        return 0;
    const CMTime now = item.currentTime;
    if (!CMTIME_IS_NUMERIC(now))
        return 0;
    const double seconds = CMTimeGetSeconds(now);
    return seconds > 0.0 ? static_cast<uint32_t>(seconds * 1000.0) : 0;
}

void KisakVideo_SetPaused(KisakVideoPlayer *handle, int paused)
{
    if (!handle)
        return;
    if (paused)
        [handle->player pause];
    else
        [handle->player play];
}

// Minimal video playback API over AVFoundation, kept free of engine headers so the
// Objective-C++ implementation never sees the Windows compatibility prelude.
// Used by ports/ios/engine/cinematic_apple.cpp to play the converted video/*.mp4.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KisakVideoPlayer KisakVideoPlayer;

// One decoded frame, planar 8-bit 4:2:0. The planes stay valid until the next
// KisakVideo_NextFrame or KisakVideo_Close on the same player.
typedef struct KisakVideoFrame
{
    const uint8_t *planes[3]; // Y, Cb, Cr
    size_t strides[3];
    int widths[3];
    int heights[3];
} KisakVideoFrame;

// Returns NULL when the file is missing or cannot be opened.
KisakVideoPlayer *KisakVideo_Open(const char *path, float volume);
void KisakVideo_Close(KisakVideoPlayer *player);
// 1 when a new frame was decoded and written to `frame`, 0 when the current frame still stands.
int KisakVideo_NextFrame(KisakVideoPlayer *player, KisakVideoFrame *frame);
int KisakVideo_AtEnd(const KisakVideoPlayer *player);
// Playback position in milliseconds, for subtitle timing.
uint32_t KisakVideo_TimeMsec(const KisakVideoPlayer *player);
void KisakVideo_SetPaused(KisakVideoPlayer *player, int paused);

#ifdef __cplusplus
}
#endif

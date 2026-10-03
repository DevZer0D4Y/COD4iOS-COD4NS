// Cinematic playback for the Apple port, replacing src/gfx_d3d/r_cinematic.cpp.
//
// The shipped movies are Bink (.bik) and no Bink runtime exists for this platform, so playback
// goes through AVFoundation instead: ports/ios/scripts/convert_videos.sh transcodes the game's
// video/*.bik files to video/*.mp4 alongside the originals, and video_player_apple.mm plays those.
// Decoded frames are planar 4:2:0, which is exactly what the stock "cinematic" material samples:
// the three planes are uploaded into the CINEMATIC_Y/CR/CB code images and drawn with the same
// letterboxed full-screen quad the Bink path used, so nothing else in the engine changes.
//
// A movie with no converted .mp4 reports finished immediately - the same outcome as a failed
// BinkOpen, so menus and level transitions carry on instead of stalling.

#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <universal/com_files.h>
#include <gfx_d3d/r_cinematic.h>
#include <gfx_d3d/r_image.h>
#include <gfx_d3d/r_state.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/rb_state.h>

#include "video_player_apple.h"

#include <mutex>
#include <string>
#include <vector>
#include <unistd.h>

// ui_shared.cpp reads the playing movie's name and position from here for subtitles; the rest of
// the Bink-era state in this struct stays zeroed.
CinematicGlob cinematicGlob;

namespace {

struct Plane
{
    GfxImage image{};
    int width = 0;
    int height = 0;
};

Plane s_planeY, s_planeCb, s_planeCr;
// R_Cinematic_UpdateFrame runs on the render thread while StopPlayback/Shutdown come from the
// main thread, so the player handle needs a lock: closing it twice freed it twice.
std::mutex s_playerMutex;
KisakVideoPlayer *s_player = nullptr;
bool s_haveFrame = false;
bool s_finished = true;
bool s_paused = false;
float s_volume = 1.0f;
std::string s_currentName;
std::string s_nextName;
uint32_t s_nextFlags = 0;
std::string s_pendingName;
uint32_t s_pendingFlags = 0;
bool s_pendingValid = false;

void ReleasePlane(Plane &plane)
{
    if (plane.image.texture.basemap)
        Image_Release(&plane.image);
    memset(&plane.image, 0, sizeof(plane.image));
    plane.width = 0;
    plane.height = 0;
}

// One 8-bit single-channel image per component, matching what the cinematic material samples.
void SetupPlane(Plane &plane, const char *name, int width, int height)
{
    if (plane.width == width && plane.height == height && plane.image.texture.basemap)
        return;
    ReleasePlane(plane);
    plane.image.name = name;
    plane.image.semantic = TS_COLOR_MAP;
    plane.image.category = IMG_CATEGORY_TEMP;
    plane.image.track = 4;
    plane.image.mapType = MAPTYPE_2D;
    Image_Setup(&plane.image, width, height, 1, IMG_FLAG_NOPICMIP | IMG_FLAG_NOMIPMAPS | IMG_FLAG_DYNAMIC, D3DFMT_L8);
    plane.width = width;
    plane.height = height;
}

void UploadPlane(Plane &plane, const uint8_t *src, size_t stride, int width, int height)
{
    if (!plane.image.texture.basemap || !src)
        return;
    if (stride == static_cast<size_t>(width))
    {
        Image_UploadData(&plane.image, D3DFMT_L8, static_cast<_D3DCUBEMAP_FACES>(0), 0, const_cast<uint8_t *>(src));
        return;
    }
    // Image_UploadData wants tightly packed rows; CoreVideo planes are row-padded.
    static std::vector<uint8_t> packed;
    packed.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int row = 0; row < height; ++row)
        memcpy(&packed[static_cast<size_t>(row) * width], src + static_cast<size_t>(row) * stride, width);
    Image_UploadData(&plane.image, D3DFMT_L8, static_cast<_D3DCUBEMAP_FACES>(0), 0, packed.data());
}

void CloseMovie()
{
    KisakVideoPlayer *player = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_playerMutex);
        player = s_player;
        s_player = nullptr;
    }
    if (player)
        KisakVideo_Close(player);
    s_haveFrame = false;
    s_paused = false;
    s_currentName.clear();
    cinematicGlob.currentCinematicName[0] = 0;
    cinematicGlob.timeInMsec = 0;
}

// Absolute path of video/<name>.mp4, or an empty string when it hasn't been converted.
std::string ResolveMoviePath(const char *name)
{
    if (!name || !*name)
        return {};
    char qpath[MAX_QPATH * 2];
    Com_sprintf(qpath, sizeof(qpath), "video/%s.mp4", name);
    const char *bases[2] = {
        fs_homepath ? fs_homepath->current.string : nullptr,
        fs_basepath ? fs_basepath->current.string : nullptr,
    };
    for (const char *base : bases)
    {
        if (!base || !*base)
            continue;
        char ospath[MAX_OSPATH];
        FS_BuildOSPath(base, "main", qpath, ospath);
        if (access(ospath, R_OK) == 0)
            return ospath;
    }
    return {};
}

bool OpenMovie(const char *name, uint32_t playbackFlags, float volume)
{
    CloseMovie();
    const std::string path = ResolveMoviePath(name);
    if (path.empty())
    {
        Com_PrintWarning(CON_CHANNEL_GFX, "Cinematic '%s': video/%s.mp4 not found; skipping movie.\n", name, name);
        return false;
    }
    KisakVideoPlayer *player = KisakVideo_Open(path.c_str(), volume);
    {
        std::lock_guard<std::mutex> lock(s_playerMutex);
        s_player = player;
    }
    if (!s_player)
    {
        Com_PrintWarning(CON_CHANNEL_GFX, "Cinematic '%s': could not open %s.\n", name, path.c_str());
        return false;
    }
    s_currentName = name;
    I_strncpyz(cinematicGlob.currentCinematicName, name, sizeof(cinematicGlob.currentCinematicName));
    cinematicGlob.timeInMsec = 0;
    s_haveFrame = false;
    s_paused = false;
    Com_Printf(CON_CHANNEL_GFX, "Cinematic '%s' playing (flags %u)\n", name, playbackFlags);
    return true;
}

// The cinematic material is part of the loading screen, so these four code images must always be
// valid - the renderer errors out ("Tried to use 'cinematicY' when it isn't valid") on a null one.
// With no decoded frame the planes fall back to black luma and neutral chroma, which samples black.
void PublishImages()
{
    const bool haveFrame = s_haveFrame && s_planeY.image.texture.basemap;
    gfxCmdBufInput.codeImages[TEXTURE_SRC_CODE_CINEMATIC_Y] = haveFrame ? &s_planeY.image : rgp.blackImage;
    gfxCmdBufInput.codeImages[TEXTURE_SRC_CODE_CINEMATIC_CR] = haveFrame ? &s_planeCr.image : rgp.grayImage;
    gfxCmdBufInput.codeImages[TEXTURE_SRC_CODE_CINEMATIC_CB] = haveFrame ? &s_planeCb.image : rgp.grayImage;
    gfxCmdBufInput.codeImages[TEXTURE_SRC_CODE_CINEMATIC_A] = rgp.whiteImage;
}

} // namespace

void __cdecl R_Cinematic_Init()
{
    s_finished = true;
    PublishImages();
}

void __cdecl R_Cinematic_Init_NULL()
{
}

void R_Cinematic_ReserveMemory()
{
}

void __cdecl R_Cinematic_Shutdown()
{
    CloseMovie();
    ReleasePlane(s_planeY);
    ReleasePlane(s_planeCb);
    ReleasePlane(s_planeCr);
}

void __cdecl R_Cinematic_StartPlayback(char *name, uint32_t playbackFlags, float volume)
{
    if (!name || !*name)
        return;
    // Opened from UpdateFrame so texture creation happens on the thread that owns the device.
    s_pendingName = name;
    s_pendingFlags = playbackFlags;
    s_pendingValid = true;
    s_volume = volume;
    s_finished = false;
}

void __cdecl R_Cinematic_StartNextPlayback()
{
    if (s_nextName.empty())
        return;
    s_pendingName = s_nextName;
    s_pendingFlags = s_nextFlags;
    s_pendingValid = true;
    s_finished = false;
    s_nextName.clear();
    s_nextFlags = 0;
}

void __cdecl R_Cinematic_StopPlayback()
{
    if (s_player)
        Com_Printf(CON_CHANNEL_GFX, "Cinematic '%s' stopped by engine\n", s_currentName.c_str());
    s_pendingValid = false;
    s_pendingName.clear();
    s_finished = true;
    CloseMovie();
}

void __cdecl R_Cinematic_UpdateFrame()
{
    PublishImages();
    if (s_pendingValid)
    {
        const std::string name = s_pendingName;
        const uint32_t flags = s_pendingFlags;
        s_pendingValid = false;
        s_pendingName.clear();
        if (!OpenMovie(name.c_str(), flags, s_volume))
        {
            s_finished = true;
            return;
        }
    }
    std::lock_guard<std::mutex> lock(s_playerMutex); // the main thread may close the player
    if (!s_player || s_paused)
        return;

    KisakVideoFrame frame;
    if (KisakVideo_NextFrame(s_player, &frame))
    {
        SetupPlane(s_planeY, "cinematic_y", frame.widths[0], frame.heights[0]);
        SetupPlane(s_planeCb, "cinematic_cb", frame.widths[1], frame.heights[1]);
        SetupPlane(s_planeCr, "cinematic_cr", frame.widths[2], frame.heights[2]);
        UploadPlane(s_planeY, frame.planes[0], frame.strides[0], frame.widths[0], frame.heights[0]);
        UploadPlane(s_planeCb, frame.planes[1], frame.strides[1], frame.widths[1], frame.heights[1]);
        UploadPlane(s_planeCr, frame.planes[2], frame.strides[2], frame.widths[2], frame.heights[2]);
        s_haveFrame = true;
        PublishImages();
    }
    cinematicGlob.timeInMsec = KisakVideo_TimeMsec(s_player);
    if (KisakVideo_AtEnd(s_player))
    {
        Com_Printf(CON_CHANNEL_GFX, "Cinematic '%s' finished at %u ms\n", s_currentName.c_str(), cinematicGlob.timeInMsec);
        s_finished = true;
        KisakVideoPlayer *player = s_player;
        s_player = nullptr;
        KisakVideo_Close(player);
        s_haveFrame = false;
        s_paused = false;
        s_currentName.clear();
        cinematicGlob.currentCinematicName[0] = 0;
        cinematicGlob.timeInMsec = 0;
    }
}

void R_Cinematic_UpdateRendererImages()
{
    PublishImages();
}

void __cdecl R_Cinematic_SyncNow()
{
}

void __cdecl R_Cinematic_DrawStretchPic_Letterboxed()
{
    const float width = static_cast<float>(vidConfig.displayWidth);
    const float height = static_cast<float>(vidConfig.displayHeight);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (!s_haveFrame)
    {
        R_AddCmdDrawStretchPic(0.0f, 0.0f, width, height, 0.0f, 0.0f, 1.0f, 1.0f, black, rgp.whiteMaterial);
        return;
    }
    PublishImages();
    // The 16:9 letterbox of the original playback path.
    float movieHeight = width * vidConfig.aspectRatioDisplayPixel / 1.7777778f;
    if (height < movieHeight)
        movieHeight = height;
    const float barHeight = (height - movieHeight) * 0.5f;
    R_AddCmdDrawStretchPic(0.0f, 0.0f, width, barHeight, 0.0f, 0.0f, 1.0f, 1.0f, black, rgp.whiteMaterial);
    R_AddCmdDrawStretchPic(0.0f, height - barHeight, width, barHeight, 0.0f, 0.0f, 1.0f, 1.0f, black, rgp.whiteMaterial);
    R_AddCmdDrawStretchPic(0.0f, barHeight, width, movieHeight, 0.0f, 0.0f, 1.0f, 1.0f, colorWhite, rgp.cinematicMaterial);
}

bool __cdecl R_Cinematic_IsFinished()
{
    return s_finished && !s_pendingValid;
}

bool __cdecl R_Cinematic_IsStarted()
{
    return !R_Cinematic_IsFinished() && (s_player != nullptr || s_pendingValid);
}

bool R_Cinematic_IsPending()
{
    return s_pendingValid;
}

bool __cdecl R_Cinematic_IsNextReady()
{
    // Only true when a queued movie is actually waiting: CG_DrawCinematic treats a "ready"
    // answer as "a cinematic owns the screen" and would letterbox over the world every frame.
    return !s_nextName.empty();
}

bool __cdecl R_Cinematic_IsUnderrun()
{
    return false;
}

void __cdecl R_Cinematic_BeginLostDevice()
{
    if (s_player)
        KisakVideo_SetPaused(s_player, 1);
}

void __cdecl R_Cinematic_EndLostDevice()
{
    if (s_player && !s_paused)
        KisakVideo_SetPaused(s_player, 0);
}

void __cdecl R_Cinematic_SetPaused(CinematicEnum paused)
{
    s_paused = paused == CINEMATIC_PAUSED;
    if (s_player)
        KisakVideo_SetPaused(s_player, s_paused ? 1 : 0);
}

void R_Cinematic_SetNextPlayback(const char *name, uint32_t playbackFlags)
{
    s_nextName = name ? name : "";
    s_nextFlags = playbackFlags;
}

void R_Cinematic_UnsetNextPlayback()
{
    s_nextName.clear();
    s_nextFlags = 0;
}

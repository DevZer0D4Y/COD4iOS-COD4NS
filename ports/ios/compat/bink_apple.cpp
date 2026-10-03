// Bink video on Apple platforms. RAD ships no Bink runtime for iOS in this
// project, so the playback API reports every movie as unavailable: BinkOpen
// fails with an error the engine logs, and it continues without the video.
// A real decoder (FFmpeg's Bink support) will replace these entry points.

#include <universal/q_shared.h>
#include <gfx_d3d/r_cinematic.h>
#include <qcommon/qcommon.h>

static char s_binkError[128] = "Bink video playback is not available on this platform";

RADEXPFUNC char PTR4* RADEXPLINK BinkGetError(void)
{
    // BinkOpen logs why video is unavailable; the engine asserts on any
    // non-empty Bink error, so report none here.
    static char noError[] = "";
    return noError;
}

RADEXPFUNC HBINK RADEXPLINK BinkOpen(const char PTR4* name, U32 flags)
{
    (void)flags;
    Com_Printf(CON_CHANNEL_GFX, "Cinematic '%s' skipped: %s.\n", name ? name : "?", s_binkError);
    return 0;
}

RADEXPFUNC void RADEXPLINK BinkClose(HBINK bnk) { (void)bnk; }
RADEXPFUNC S32 RADEXPLINK BinkDoFrame(HBINK bnk) { (void)bnk; return 0; }
RADEXPFUNC void RADEXPLINK BinkNextFrame(HBINK bnk) { (void)bnk; }
RADEXPFUNC S32 RADEXPLINK BinkWait(HBINK bnk) { (void)bnk; return 0; }
RADEXPFUNC S32 RADEXPLINK BinkPause(HBINK bnk, S32 pause) { (void)bnk; return pause; }

RADEXPFUNC void RADEXPLINK BinkGetRealtime(HBINK bink, BINKREALTIME PTR4* run, U32 frames)
{
    (void)bink; (void)frames;
    if (run)
        memset(run, 0, sizeof(*run));
}

RADEXPFUNC void RADEXPLINK BinkSetMemory(BINKMEMALLOC a, BINKMEMFREE f) { (void)a; (void)f; }
RADEXPFUNC void RADEXPLINK BinkSetIOSize(U32 iosize) { (void)iosize; }
RADEXPFUNC void RADEXPLINK BinkSetSoundTrack(U32 total_tracks, U32 PTR4* tracks) { (void)total_tracks; (void)tracks; }
RADEXPFUNC S32 RADEXPLINK BinkSetSoundSystem(BINKSNDSYSOPEN open, UINTa param) { (void)open; (void)param; return 0; }

RADEXPFUNC void RADEXPLINK BinkSetMixBinVolumes(HBINK bnk, U32 trackid, U32 PTR4* vol_mix_bins, S32 PTR4* volumes, U32 total)
{
    (void)bnk; (void)trackid; (void)vol_mix_bins; (void)volumes; (void)total;
}

RADEXPFUNC S32 RADEXPLINK BinkControlBackgroundIO(HBINK bink, U32 control) { (void)bink; (void)control; return 0; }

RADEXPFUNC void RADEXPLINK BinkGetFrameBuffersInfo(HBINK bink, BINKFRAMEBUFFERS * fbset)
{
    (void)bink;
    if (fbset)
        memset(fbset, 0, sizeof(*fbset));
}

RADEXPFUNC void RADEXPLINK BinkRegisterFrameBuffers(HBINK bink, BINKFRAMEBUFFERS * fbset) { (void)bink; (void)fbset; }

// Texture helpers from binktextures.h. With no movie ever open there are no
// textures to create, draw or lock.
RADDEFFUNC S32 Create_Bink_textures(if_used_3d_device BINKTEXTURESET * set_textures) { (void)set_textures; return 0; }
RADDEFFUNC void Free_Bink_textures(if_used_3d_device BINKTEXTURESET * set_textures) { (void)set_textures; }
RADDEFFUNC void Draw_Bink_textures(if_used_3d_device BINKTEXTURESET * set_textures, U32 width, U32 height,
                                   F32 x_offset, F32 y_offset, F32 x_scale, F32 y_scale, F32 alpha_level,
                                   S32 is_premultiplied_alpha)
{
    (void)set_textures; (void)width; (void)height; (void)x_offset; (void)y_offset;
    (void)x_scale; (void)y_scale; (void)alpha_level; (void)is_premultiplied_alpha;
}
RADDEFFUNC void Lock_Bink_textures(BINKTEXTURESET * set_textures) { (void)set_textures; }
RADDEFFUNC void Unlock_Bink_textures(LPDIRECT3DDEVICE9 d3d_device, BINKTEXTURESET * set_textures, HBINK Bink)
{
    (void)d3d_device; (void)set_textures; (void)Bink;
}

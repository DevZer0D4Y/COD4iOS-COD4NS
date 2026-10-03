// Xbox 360-style button icons for controller prompts.
//
// The PC release ships almost no console button art (only images/ui_button_xenon_a_16x16 and a
// D-pad/trigger pair), and no materials for any of it, so the glyphs are drawn here instead:
// a coloured disc or pad with the button's label on it, rendered once into a runtime image and
// wrapped in a material cloned from "white". Menu and HUD text then carries the icon inline
// through the engine's existing hud-icon escape (see KisakApple_ControllerIconEscape and
// RB_DrawHudIcon), so "[{+attack}] Fire" draws the RT glyph exactly where the key name went.
//
// Colours, labels and layout follow the Xbox 360 pad the game shipped prompts for: green A,
// red B, blue X, amber Y, grey bumpers, triggers and stick clicks (LS/RS), START and BACK for
// the two small buttons - not the Xbox One MENU/VIEW names - and the D-pad shown as a dark pad
// with the pressed direction lit.

#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <gfx_d3d/r_image.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_state.h>
#include <gfx_d3d/rb_backend.h>

#include "controller_input.h"
#include "controller_icons.h"

#include <cmath>
#include <cstring>

namespace {

using namespace kisak::controller;

constexpr int ICON_SIZE = 64;
constexpr int SUPERSAMPLE = 4; // the discs and pads are drawn at 4x and boxed down

struct IconAsset
{
    GfxImage image{};
    Material material{};
    MaterialTextureDef texture{};
    int width = 0; // pixels; square for face buttons, wider for LB/RT/MENU-style labels
    bool built = false;
};

IconAsset s_icons[Count];
char s_iconNames[Count][32];

// 5x7 glyphs for the characters button labels use. Each byte is one row, bit 4 is the left column.
struct GlyphBits { char letter; uint8_t rows[7]; };
const GlyphBits s_glyphs[] = {
    {'A', {0x04, 0x0A, 0x11, 0x11, 0x1F, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
};

const GlyphBits *FindGlyph(char letter)
{
    for (const GlyphBits &glyph : s_glyphs)
        if (glyph.letter == letter)
            return &glyph;
    return nullptr;
}

// Texel byte order for the icon images. The Metal backend hands D3DFMT_A8R8G8B8 data to an
// RGBA8 texture, so the bytes go out in R, G, B, A order rather than D3D9's B, G, R, A.
struct Rgba { uint8_t r, g, b, a; };

constexpr int ICON_MAX_WIDTH = ICON_SIZE * 3; // MENU/VIEW are the widest labels

struct Canvas
{
    Rgba pixels[ICON_MAX_WIDTH * SUPERSAMPLE * ICON_SIZE * SUPERSAMPLE]{};
    int size = ICON_SIZE * SUPERSAMPLE;  // height in supersampled pixels
    int width = ICON_SIZE * SUPERSAMPLE; // width in supersampled pixels

    void Fill(int x, int y, Rgba color)
    {
        if (x < 0 || y < 0 || x >= width || y >= size)
            return;
        pixels[y * ICON_MAX_WIDTH * SUPERSAMPLE + x] = color;
    }
    void Disc(float cx, float cy, float radius, Rgba color)
    {
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < width; ++x)
            {
                const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                if (dx * dx + dy * dy <= radius * radius)
                    Fill(x, y, color);
            }
    }
    void RoundedBox(float x0, float y0, float x1, float y1, float radius, Rgba color)
    {
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < width; ++x)
            {
                const float px = x + 0.5f, py = y + 0.5f;
                if (px < x0 || px > x1 || py < y0 || py > y1)
                    continue;
                const float qx = fmaxf(x0 + radius - px, px - (x1 - radius));
                const float qy = fmaxf(y0 + radius - py, py - (y1 - radius));
                if (qx > 0.0f && qy > 0.0f && qx * qx + qy * qy > radius * radius)
                    continue;
                Fill(x, y, color);
            }
    }
    // Label centred on (cx, cy); each glyph pixel becomes a scale x scale block.
    void Text(const char *label, float cx, float cy, float scale, Rgba color)
    {
        const int length = static_cast<int>(strlen(label));
        if (!length)
            return;
        const float glyphWidth = 5.0f * scale, spacing = scale;
        const float totalWidth = length * glyphWidth + (length - 1) * spacing;
        float penX = cx - totalWidth * 0.5f;
        const float penY = cy - 7.0f * scale * 0.5f;
        for (int index = 0; index < length; ++index)
        {
            const GlyphBits *glyph = FindGlyph(label[index]);
            if (glyph)
            {
                for (int row = 0; row < 7; ++row)
                    for (int column = 0; column < 5; ++column)
                    {
                        if (!(glyph->rows[row] & (0x10 >> column)))
                            continue;
                        const int x0 = static_cast<int>(penX + column * scale);
                        const int y0 = static_cast<int>(penY + row * scale);
                        for (int y = 0; y < static_cast<int>(scale); ++y)
                            for (int x = 0; x < static_cast<int>(scale); ++x)
                                Fill(x0 + x, y0 + y, color);
                    }
            }
            penX += glyphWidth + spacing;
        }
    }
};

// Box-filter the supersampled canvas down to the final ICON_SIZE image.
void Resolve(const Canvas &canvas, Rgba *out, int outWidth)
{
    for (int y = 0; y < ICON_SIZE; ++y)
        for (int x = 0; x < outWidth; ++x)
        {
            int sums[4] = {};
            for (int sy = 0; sy < SUPERSAMPLE; ++sy)
                for (int sx = 0; sx < SUPERSAMPLE; ++sx)
                {
                    const Rgba &pixel =
                        canvas.pixels[(y * SUPERSAMPLE + sy) * ICON_MAX_WIDTH * SUPERSAMPLE + x * SUPERSAMPLE + sx];
                    sums[0] += pixel.r; sums[1] += pixel.g; sums[2] += pixel.b; sums[3] += pixel.a;
                }
            const int samples = SUPERSAMPLE * SUPERSAMPLE;
            Rgba &texel = out[y * outWidth + x];
            texel.r = static_cast<uint8_t>(sums[0] / samples);
            texel.g = static_cast<uint8_t>(sums[1] / samples);
            texel.b = static_cast<uint8_t>(sums[2] / samples);
            texel.a = static_cast<uint8_t>(sums[3] / samples);
        }
}

constexpr Rgba RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { return Rgba{r, g, b, a}; }

const Rgba kWhite = RGBA(255, 255, 255);
const Rgba kOutline = RGBA(24, 24, 24, 220);
const Rgba kGrey = RGBA(216, 216, 216);
const Rgba kDarkGrey = RGBA(64, 64, 64);

struct FaceStyle { Rgba fill; const char *label; };

FaceStyle StyleFor(int button)
{
    switch (button)
    {
    case South: return {RGBA(80, 168, 58), "A"};   // 360 green
    case East:  return {RGBA(196, 56, 44), "B"};   // red
    case West:  return {RGBA(40, 96, 190), "X"};   // blue
    case North: return {RGBA(222, 176, 40), "Y"};  // amber
    case L1:    return {kGrey, "LB"};
    case R1:    return {kGrey, "RB"};
    case L2:    return {kGrey, "LT"};
    case R2:    return {kGrey, "RT"};
    case L3:    return {kGrey, "LS"};
    case R3:    return {kGrey, "RS"};
    case Menu:  return {kGrey, "START"};
    case Options: return {kGrey, "BACK"};
    default: return {kGrey, ""};
    }
}

// A one-letter face button stays a circle; longer labels (LB, RT, MENU) get a rounded pill as
// wide as the text needs, so they stay readable at HUD size.
int IconWidthFor(int button)
{
    if (button >= Up)
        return ICON_SIZE;
    const size_t length = strlen(StyleFor(button).label);
    if (length <= 1)
        return ICON_SIZE;
    return length >= 4 ? ICON_SIZE * 5 / 2 : ICON_SIZE * 3 / 2;
}

void DrawFace(Canvas &canvas, int button)
{
    const FaceStyle style = StyleFor(button);
    const float centreY = canvas.size * 0.5f;
    const float centreX = canvas.width * 0.5f;
    const bool round = button <= North;
    if (round)
    {
        canvas.Disc(centreX, centreY, canvas.size * 0.46f, kOutline);
        canvas.Disc(centreX, centreY, canvas.size * 0.42f, style.fill);
    }
    else
    {
        const float inset = canvas.size * 0.06f;
        canvas.RoundedBox(inset, canvas.size * 0.18f, canvas.width - inset, canvas.size * 0.82f,
                          canvas.size * 0.22f, kOutline);
        canvas.RoundedBox(inset * 1.8f, canvas.size * 0.22f, canvas.width - inset * 1.8f, canvas.size * 0.78f,
                          canvas.size * 0.20f, style.fill);
    }
    const bool lightFill = style.fill.r + style.fill.g + style.fill.b > 520;
    const Rgba labelColor = lightFill ? kDarkGrey : kWhite;
    const size_t length = strlen(style.label);
    const float scale = length >= 4 ? canvas.size / 22.0f : (length == 1 ? canvas.size / 12.0f : canvas.size / 16.0f);
    canvas.Text(style.label, centreX, centreY, scale, labelColor);
}

// Dark pad with one direction lit, matching how the 360 prompts show D-pad entries.
void DrawDpad(Canvas &canvas, int button)
{
    const float size = static_cast<float>(canvas.size);
    const float arm = size * 0.22f, centre = size * 0.5f;
    canvas.RoundedBox(centre - arm, size * 0.08f, centre + arm, size * 0.92f, arm * 0.5f, kOutline);
    canvas.RoundedBox(size * 0.08f, centre - arm, size * 0.92f, centre + arm, arm * 0.5f, kOutline);
    canvas.RoundedBox(centre - arm * 0.8f, size * 0.12f, centre + arm * 0.8f, size * 0.88f, arm * 0.4f, kDarkGrey);
    canvas.RoundedBox(size * 0.12f, centre - arm * 0.8f, size * 0.88f, centre + arm * 0.8f, arm * 0.4f, kDarkGrey);
    const float lit = size * 0.30f;
    switch (button)
    {
    case Up:    canvas.RoundedBox(centre - arm * 0.8f, size * 0.12f, centre + arm * 0.8f, lit, arm * 0.4f, kWhite); break;
    case Down:  canvas.RoundedBox(centre - arm * 0.8f, size - lit, centre + arm * 0.8f, size * 0.88f, arm * 0.4f, kWhite); break;
    case Left:  canvas.RoundedBox(size * 0.12f, centre - arm * 0.8f, lit, centre + arm * 0.8f, arm * 0.4f, kWhite); break;
    case Right: canvas.RoundedBox(size - lit, centre - arm * 0.8f, size * 0.88f, centre + arm * 0.8f, arm * 0.4f, kWhite); break;
    default: break;
    }
}

bool BuildIcon(int button)
{
    IconAsset &icon = s_icons[button];
    if (icon.built)
        return true;
    if (!rgp.whiteMaterial || !rgp.whiteMaterial->textureTable || !rgp.whiteMaterial->textureCount)
        return false; // renderer assets not loaded yet

    static Canvas canvas; // reused for every icon
    memset(canvas.pixels, 0, sizeof(canvas.pixels));
    icon.width = IconWidthFor(button);
    canvas.width = icon.width * SUPERSAMPLE;
    if (button >= Up)
        DrawDpad(canvas, button);
    else
        DrawFace(canvas, button);

    static Rgba resolved[ICON_MAX_WIDTH * ICON_SIZE];
    Resolve(canvas, resolved, icon.width);

    Com_sprintf(s_iconNames[button], sizeof(s_iconNames[button]), "$gamepad_icon_%d", button);
    icon.image.name = s_iconNames[button];
    icon.image.semantic = TS_COLOR_MAP;
    icon.image.category = IMG_CATEGORY_TEMP;
    icon.image.track = 4;
    icon.image.mapType = MAPTYPE_2D;
    Image_Setup(&icon.image, icon.width, ICON_SIZE, 1, IMG_FLAG_NOPICMIP | IMG_FLAG_NOMIPMAPS, D3DFMT_A8R8G8B8);
    Image_UploadData(&icon.image, D3DFMT_A8R8G8B8, static_cast<_D3DCUBEMAP_FACES>(0), 0,
                     reinterpret_cast<uint8_t *>(resolved));

    // A copy of "white" (a plain unlit 2D technique set) with its colour map pointed at the icon.
    icon.material = *rgp.whiteMaterial;
    icon.texture = rgp.whiteMaterial->textureTable[0];
    icon.texture.u.image = &icon.image;
    icon.material.info.name = s_iconNames[button];
    icon.material.textureTable = &icon.texture;
    icon.material.textureCount = 1;
    icon.built = true;
    return true;
}

} // namespace

const Material *KisakApple_ControllerIconMaterial(int button)
{
    if (button < 0 || button >= Count)
        return nullptr;
    if (!BuildIcon(button))
        return nullptr;
    return &s_icons[button].material;
}

unsigned KisakApple_ControllerIconEscape(int button, char *out, unsigned capacity)
{
    if (!out || button < 0 || button >= Count || capacity < 1 + CONTXTCMD_GAMEPAD_ICON_BYTES + 1)
        return 0;
    if (!KisakApple_ControllerIconMaterial(button))
        return 0;
    // '^' then the escape RB_DrawHudIcon reads: type, width, height, icon index. The index is
    // stored +1 so no byte of the escape is NUL, which would cut the surrounding string short.
    unsigned index = 0;
    out[index++] = '^';
    out[index++] = CONTXTCMD_TYPE_GAMEPAD_ICON;
    // Width and height are (pixelHeight * (value - 16) + 16) / 32 in the renderer, so 16 + 32
    // gives one line's height: a square glyph that sits on the text baseline.
    const int iconWidth = s_icons[button].width ? s_icons[button].width : ICON_SIZE;
    const int widthByte = 16 + 32 * iconWidth / ICON_SIZE;
    out[index++] = static_cast<char>(widthByte < 255 ? widthByte : 254);
    out[index++] = 16 + 32;
    out[index++] = static_cast<char>(button + 1);
    out[index] = 0;
    return index;
}

void KisakApple_ControllerIconsRelease()
{
    for (IconAsset &icon : s_icons)
    {
        if (!icon.built)
            continue;
        if (icon.image.texture.basemap)
            Image_Release(&icon.image);
        memset(&icon.image, 0, sizeof(icon.image));
        icon.built = false;
    }
}

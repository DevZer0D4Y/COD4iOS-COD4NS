// Xbox 360-style button glyphs for controller prompts (see controller_icons.cpp).
#pragma once

struct Material;

// Icon material for a kisak::controller::Button, or nullptr before the renderer's
// built-in materials exist.
const Material *KisakApple_ControllerIconMaterial(int button);
// Writes the engine's inline hud-icon escape for a button into `out` (NUL terminated) and
// returns its length, or 0 when the icon isn't available or the buffer is too small.
unsigned KisakApple_ControllerIconEscape(int button, char *out, unsigned capacity);
void KisakApple_ControllerIconsRelease();

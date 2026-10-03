#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kisak::controller {
enum Button { South, East, West, North, L1, R1, L2, R2, L3, R3, Menu, Options, Up, Down, Left, Right, Count };
struct Snapshot {
    bool connected = false;
    bool touch = false;
    float lookDeltaX = 0, lookDeltaY = 0;
    uint32_t buttons = 0;
    float leftX = 0, leftY = 0, rightX = 0, rightY = 0;
    float leftTrigger = 0, rightTrigger = 0;
    char labels[Count][64]{};
};
inline void Deadzone(float &x, float &y, float inner = 0.18f, float outer = 0.0f) {
    if (!std::isfinite(x) || !std::isfinite(y)) { x = y = 0; return; }
    inner = std::isfinite(inner) ? std::clamp(inner, 0.0f, 1.0f) : 0.18f;
    outer = std::isfinite(outer) ? std::clamp(outer, 0.0f, 1.0f) : 0.0f;
    const float length = std::hypot(x, y);
    if (!std::isfinite(length) || length <= inner || inner >= 1.0f) { x = y = 0; return; }
    const float span = std::max(1.0f - outer - inner, 0.001f);
    const float scale = std::min((length - inner) / span, 1.0f) / length;
    x *= scale; y *= scale;
}
// Hysteresis avoids repeated fire/ADS edges near the trigger threshold.
inline bool TriggerPressed(float value, bool held, float threshold) {
    if (!std::isfinite(value)) return false;
    threshold = std::clamp(threshold, 0.0f, 1.0f);
    return value > (held ? std::max(0.0f, threshold - 0.05f) : threshold);
}
inline uint32_t MenuDirection(float x, float y, float threshold) {
    if (!std::isfinite(x) || !std::isfinite(y)) return 0;
    threshold = std::clamp(threshold, 0.05f, 1.0f);
    // A diagonal must select one item, not two in the same frame.
    if (std::fabs(y) >= std::fabs(x))
        return std::fabs(y) > threshold ? 1u << (y > 0 ? Up : Down) : 0;
    return std::fabs(x) > threshold ? 1u << (x > 0 ? Right : Left) : 0;
}
struct Edges { uint32_t pressed = 0, released = 0; };
// A held button must be released before it can act in a new input context.
// In particular, closing Pause must not also fire/jump in the game underneath.
class ButtonState {
    uint32_t held = 0, blocked = 0;
    bool connected = false, menu = false;
public:
    Edges Update(uint32_t buttons, bool isConnected, bool inMenu) {
        Edges result;
        if (connected != isConnected || menu != inMenu) {
            result.released = held;
            held = 0;
            blocked = buttons;
        }
        connected = isConnected; menu = inMenu;
        if (!connected) buttons = 0;
        blocked &= buttons;
        const uint32_t next = buttons & ~blocked;
        result.pressed = next & ~held;
        result.released |= held & ~next;
        held = next;
        return result;
    }
};
}

struct usercmd_s;
void KisakApple_ControllerSubmit(const kisak::controller::Snapshot &snapshot);
void KisakApple_ControllerRegisterDvars();
void KisakApple_ControllerFrame();
void KisakApple_ControllerMove(usercmd_s *cmd, float seconds);
bool KisakApple_ControllerBinding(const char *command, char *label, unsigned capacity);

// UI reads an atomic context: 0 menus, 1 gameplay, 2 cinematic.
int KisakApple_ControllerTouchContext();

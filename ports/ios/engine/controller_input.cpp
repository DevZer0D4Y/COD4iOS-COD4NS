#include "controller_input.h"
#include "controller_icons.h"
#include "../input/TouchControls.h"
#include <gfx_d3d/r_cinematic.h>
#include <atomic>
#include <aim_assist/aim_assist.h>
#include <client/client.h>
#ifdef KISAK_MP
#include <client_mp/client_mp.h>
#include <cgame_mp/cg_local_mp.h>
#else
#include <cgame/cg_main.h>
#include <cgame/cg_consolecmds.h>
#endif
#include <qcommon/cmd.h>
#include <qcommon/qcommon.h>
#include <ui/keycodes.h>
#include <ui/ui_shared.h>
#include <deque>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <universal/com_files.h>

void KisakApple_ControllerCursor(float dx, float dy, int button);
bool KisakApple_GetDisplaySize(int *width, int *height);

namespace {
using namespace kisak::controller;
unsigned lastCursorTime = 0;
std::mutex inputMutex;
std::deque<Snapshot> pending;
Snapshot current;
ButtonState buttonState;
bool sprintRequested = false;
std::atomic<int> touchContext{0};
float touchLookX = 0, touchLookY = 0;
struct Route { const char *command = nullptr; int key = 0; };
Route heldRoutes[Count];
bool pointerMenu = false, triggerHeld[2] = {};
int repeatButton = -1;
unsigned repeatAt = 0;
const dvar_t *sensitivity, *adsSensitivity, *aimAssist, *invertPitch, *buttonConfig, *thumbstickConfig;
// The rest of the set the shipped gamepad configs and the Gamepad options page expect. Without
// these the official buttons_*_alt.cfg / thumbstick_*.cfg files log "dvar 'gpad_...' doesn't
// exist" and their tuning is dropped on the floor.
const dvar_t *gpadEnabled, *gpadRumble, *stickDeadzoneMin, *stickDeadzoneMax, *buttonDeadzone,
             *stickPressed, *lstickDeflectMax, *rstickDeflectMax;
float StickDeadzone() { return stickDeadzoneMin ? stickDeadzoneMin->current.value : 0.18f; }
float StickPressed() { return stickPressed ? stickPressed->current.value : 0.55f; }

// The shipped console layouts. buttons_<name>_alt.cfg in the game's iw_00.iwd binds the console
// button names below to game commands; thumbstick_<name>.cfg maps the physical sticks to the
// virtual axes. Both are read at runtime so the port uses the game's own layout rather than a
// copy of it - see LoadButtonConfig/LoadThumbstickConfig.
extern const char *fallbackLabels[];
struct ButtonName { const char *name; int button; };
const ButtonName kButtonNames[] = {
    {"BUTTON_A", South}, {"BUTTON_B", East}, {"BUTTON_X", West}, {"BUTTON_Y", North},
    {"BUTTON_LSHLDR", L1}, {"BUTTON_RSHLDR", R1}, {"BUTTON_LTRIG", L2}, {"BUTTON_RTRIG", R2},
    {"BUTTON_LSTICK", L3}, {"BUTTON_RSTICK", R3}, {"BUTTON_START", Menu}, {"BUTTON_BACK", Options},
    {"DPAD_UP", Up}, {"DPAD_DOWN", Down}, {"DPAD_LEFT", Left}, {"DPAD_RIGHT", Right},
    {"APAD_UP", Up}, {"APAD_DOWN", Down}, {"APAD_LEFT", Left}, {"APAD_RIGHT", Right},
};

// Defaults matching buttons_default_alt.cfg, used until (or unless) the .cfg is read.
char commandStorage[Count][64];
const char *const defaultCommands[Count] = {
    "+gostand", "+stance", "+usereload", "weapnext", "+smoke", "+frag", "+speed_throw", "+attack",
    "+breath_sprint", "+melee", nullptr, "+scores", "+actionslot 1", "+actionslot 2", "+actionslot 3", "+actionslot 4"
};

const char *commands[Count] = {};

// Stick assignment, as bindaxis in thumbstick_*.cfg: which virtual axis each physical axis feeds
// and whether it uses the squared response curve (MAP_SQUARED) or a linear one.
enum VirtualAxis { VA_NONE, VA_SIDE, VA_FORWARD, VA_YAW, VA_PITCH };
struct AxisBinding { VirtualAxis axis = VA_NONE; bool squared = false; };
AxisBinding axisBindings[4]; // A_LSTICK_X, A_LSTICK_Y, A_RSTICK_X, A_RSTICK_Y
bool configLoaded = false;

float ApplyCurve(float value, bool squared) {
    return squared ? value * std::fabs(value) : value;
}

// "bind BUTTON_X "+usereload"" / "bindaxis A_LSTICK_X VA_SIDE MAP_SQUARED"
void ParseConfig(const char *text) {
    const char *line = text;
    while (line && *line) {
        const char *end = strchr(line, '\n');
        std::string entry(line, end ? end - line : strlen(line));
        line = end ? end + 1 : nullptr;
        const size_t comment = entry.find("//");
        if (comment != std::string::npos)
            entry.erase(comment);
        char first[32] = {}, second[64] = {}, third[64] = {};
        if (std::sscanf(entry.c_str(), " %31s %63s %63[^\r\n]", first, second, third) < 3)
            continue;
        std::string value(third);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
            value.pop_back();
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        if (!I_stricmp(first, "bind")) {
            for (const ButtonName &entryName : kButtonNames) {
                if (I_stricmp(second, entryName.name))
                    continue;
                I_strncpyz(commandStorage[entryName.button], value.c_str(), sizeof(commandStorage[0]));
                commands[entryName.button] = commandStorage[entryName.button];
            }
        } else if (!I_stricmp(first, "bindaxis")) {
            const struct { const char *name; int index; } sticks[] = {
                {"A_LSTICK_X", 0}, {"A_LSTICK_Y", 1}, {"A_RSTICK_X", 2}, {"A_RSTICK_Y", 3}};
            const struct { const char *name; VirtualAxis axis; } axes[] = {
                {"VA_SIDE", VA_SIDE}, {"VA_FORWARD", VA_FORWARD}, {"VA_YAW", VA_YAW}, {"VA_PITCH", VA_PITCH}};
            for (const auto &stick : sticks) {
                if (I_stricmp(second, stick.name))
                    continue;
                for (const auto &axis : axes) {
                    if (value.rfind(axis.name, 0) != 0)
                        continue;
                    axisBindings[stick.index].axis = axis.axis;
                    axisBindings[stick.index].squared = value.find("MAP_SQUARED") != std::string::npos;
                }
            }
        }
    }
}

void LoadConfigFile(const char *fileName) {
    void *buffer = nullptr;
    const int length = FS_ReadFile(fileName, &buffer);
    if (length <= 0 || !buffer) {
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "Controller: %s not found; using default layout\n", fileName);
        return;
    }
    std::string text(static_cast<const char *>(buffer), length);
    FS_FreeFile(static_cast<char *>(buffer));
    ParseConfig(text.c_str());
    Com_Printf(CON_CHANNEL_SYSTEM, "Controller: applied %s\n", fileName);
}

void LoadLayout() {
    for (int b = 0; b < Count; ++b) commands[b] = defaultCommands[b];
    axisBindings[0] = {VA_SIDE, true};
    axisBindings[1] = {VA_FORWARD, true};
    axisBindings[2] = {VA_YAW, false};
    axisBindings[3] = {VA_PITCH, false};
    const char *buttons = buttonConfig ? buttonConfig->current.string : "buttons_default";
    const char *sticks = thumbstickConfig ? thumbstickConfig->current.string : "thumbstick_default";
    LoadConfigFile(va("%s_alt.cfg", buttons));
    LoadConfigFile(va("%s.cfg", sticks));
    // START/BACK are engine behaviour on the console builds rather than .cfg binds.
    commands[Menu] = nullptr;      // opens the pause menu (routed as Escape)
    if (!commands[Options] || !*commands[Options])
        commands[Options] = "+scores";
    configLoaded = true;
    for (int button = 0; button < Count; ++button)
        if (commands[button])
            Com_Printf(CON_CHANNEL_SYSTEM, "Controller: %s -> %s\n", fallbackLabels[button], commands[button]);
}
const char *fallbackLabels[] = {
    "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS", "START", "BACK", "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right"
};
bool InMenu() {
    return clientUIActives[0].connectionState != CA_ACTIVE || (clientUIActives[0].keyCatchers & 0x13) != 0;
}
// The Xbox 360 menu layout: A selects, B and Back go back, Start closes, the D-pad (and either
// stick, which drives the cursor) navigates, and the bumpers step through tabs/values. Enter acts
// on whatever the cursor or the arrow keys focused, which is how the PC menus work.
Route RouteFor(int button, bool menu) {
    if (button == Menu) return {nullptr, K_ESCAPE};
#ifdef KISAK_MP
    // Opening the scoreboard activates a UI catcher. Select must keep its
    // scoreboard action there so the next click can close it.
    if (button == Options && UI_GetActiveMenu(0) == UIMENU_SCOREBOARD)
        return {"+scores", 0};
#endif
    if (menu) {
        switch (button) {
        // Left stick/D-pad select focused items; right stick opts into pointing.
        case South: return {nullptr, pointerMenu ? K_MOUSE1 : K_ENTER};
        case East: return {nullptr, K_ESCAPE};
        case Options: return {nullptr, K_ESCAPE};
        case West: return {nullptr, K_ENTER};
        case North: return {nullptr, K_TAB};
        case L1: return {nullptr, K_LEFTARROW};
        case R1: return {nullptr, K_RIGHTARROW};
        case L2: return {nullptr, K_PGUP};
        case R2: return {nullptr, K_PGDN};
        case Up: return {nullptr, K_UPARROW};
        case Down: return {nullptr, K_DOWNARROW};
        case Left: return {nullptr, K_LEFTARROW};
        case Right: return {nullptr, K_RIGHTARROW};
        default: return {};
        }
    }
    return {commands[button], 0};
}
void Emit(Route route, int button, bool down, unsigned time) {
    {
        // Bounded trace of what each press resolves to, so a "nothing is bound" report can be
        // checked against what the engine actually received.
        static int pressLog = 0;
        if (down && pressLog < 24) {
            ++pressLog;
            Com_Printf(CON_CHANNEL_SYSTEM, "Controller press: %s -> %s%s\n",
                       button >= 0 && button < Count ? fallbackLabels[button] : "?",
                       route.command ? route.command : "",
                       route.key ? va(" key %d", route.key) : (route.command ? "" : " (unbound)"));
        }
    }
    if (route.key) {
        if (route.key == K_MOUSE1) {
            KisakApple_ControllerCursor(0.0f, 0.0f, down ? 1 : 0);
            return;
        }
        CL_KeyEvent(0, route.key, down, time);
    } else if (route.command && (down || route.command[0] == '+')) {
        char text[128];
        // Unique virtual key ids preserve the engine's simultaneous-key rules.
        if (route.command[0] == '+')
            std::snprintf(text, sizeof(text), "%c%s %d %u\n", down ? '+' : '-', route.command + 1, 256 + button, time);
        else
            std::snprintf(text, sizeof(text), "%s\n", route.command);
        Cbuf_AddText(0, text);
    }
}
uint32_t EffectiveButtons(bool menu) {
    uint32_t buttons = current.buttons & ~((1u << L2) | (1u << R2));
    const float threshold = buttonDeadzone ? buttonDeadzone->current.value : 0.13f;
    triggerHeld[0] = TriggerPressed(current.leftTrigger, triggerHeld[0], threshold);
    triggerHeld[1] = TriggerPressed(current.rightTrigger, triggerHeld[1], threshold);
    if (triggerHeld[0]) buttons |= 1u << L2;
    if (triggerHeld[1]) buttons |= 1u << R2;
    if (menu) {
        buttons |= MenuDirection(current.leftX, current.leftY, StickPressed());
        if (buttons & ((1u << Up) | (1u << Down) | (1u << Left) | (1u << Right)))
            pointerMenu = false;
        else {
            float x = current.rightX, y = current.rightY;
            Deadzone(x, y, StickDeadzone());
            if (x || y) pointerMenu = true;
        }
    }
    return buttons;
}
void ToggleScores() {
    const cg_s *cg = CG_GetLocalClientGlobals(0);
    if (!cg->nextSnap) return;
    if (cg->showScores)
        CG_ScoresUp_f();
    else
        CG_ScoresDown_f();
}
void Process(unsigned time) {
    const bool menu = InMenu();
    const uint32_t buttons = EffectiveButtons(menu);
    if (menu || !current.connected || (!current.touch && gpadEnabled && !gpadEnabled->current.enabled))
        sprintRequested = false;
    const Edges edges = buttonState.Update(buttons, current.connected && (current.touch || !gpadEnabled || gpadEnabled->current.enabled), menu);
    for (int b = 0; b < Count; ++b) {
        if (edges.released & (1u << b)) {
            Emit(heldRoutes[b], b, false, time);
            heldRoutes[b] = {};
            if (repeatButton == b) repeatButton = -1;
        }
    }
    for (int b = 0; b < Count; ++b) {
        if (edges.pressed & (1u << b)) {
            heldRoutes[b] = RouteFor(b, menu);
            if (heldRoutes[b].command && !I_stricmp(heldRoutes[b].command, "+scores")) {
                ToggleScores();
                // Releasing the physical button or changing to the scoreboard
                // UI must not send -scores; only another click closes it.
                heldRoutes[b] = {};
                continue;
            }
            // Sprint is toggled by a rising edge in PM_UpdateSprint. Keep breath
            // held for scoped weapons, but send sprint for just one usercmd.
            if (!menu && heldRoutes[b].command) {
                if (!I_stricmp(heldRoutes[b].command, "+breath_sprint")) {
#ifdef KISAK_SP
                    // The training script listens for the original sprint
                    // command, before its held input is rerouted to breath.
                    Cmd_CheckNotifyForCommand(heldRoutes[b].command);
#endif
                    sprintRequested = true;
                    heldRoutes[b].command = "+holdbreath";
                } else if (!I_stricmp(heldRoutes[b].command, "+sprint")) {
#ifdef KISAK_SP
                    // The training script listens for the original sprint
                    // command, before its held input is rerouted to breath.
                    Cmd_CheckNotifyForCommand(heldRoutes[b].command);
#endif
                    sprintRequested = true;
                    heldRoutes[b] = {};
                }
            }
            Emit(heldRoutes[b], b, true, time);
            if (menu && b >= Up) { repeatButton = b; repeatAt = time + 350; }
        }
    }
    if (menu && repeatButton >= 0 && static_cast<int32_t>(time - repeatAt) >= 0) {
        Emit(heldRoutes[repeatButton], repeatButton, false, time);
        Emit(heldRoutes[repeatButton], repeatButton, true, time);
        repeatAt = time + 100;
    }
}
}

void KisakApple_ControllerSubmit(const kisak::controller::Snapshot &snapshot) {
    std::lock_guard<std::mutex> lock(inputMutex);
    // Coalesce analog samples, but retain quick button down/up transitions.
    if (!pending.empty() && pending.back().buttons == snapshot.buttons && pending.back().connected == snapshot.connected
        && pending.back().touch == snapshot.touch
        && pending.back().leftTrigger == snapshot.leftTrigger && pending.back().rightTrigger == snapshot.rightTrigger) {
        // Stick positions replace older positions; swipe distances are increments.
        // A timer sample with zero motion must not erase the preceding touch event.
        const float dx = pending.back().lookDeltaX, dy = pending.back().lookDeltaY;
        pending.back() = snapshot;
        if (snapshot.touch && snapshot.connected) {
            pending.back().lookDeltaX += dx;
            pending.back().lookDeltaY += dy;
        }
    }
    else {
        if (pending.size() >= 128) { pending.clear(); pending.emplace_back(); }
        pending.push_back(snapshot);
    }
}
// Registered from CL_InitOnceForAllClients, not lazily on the first controller frame: the
// shipped gamepad and devgui configs are exec'd during startup and would otherwise hit
// "dvar 'gpad_...' doesn't exist" and drop their values.
void KisakApple_ControllerRegisterDvars() {
    if (sensitivity) return;
        sensitivity = Dvar_RegisterFloat("gpad_sensitivity", 1.0f, 0.1f, 4.0f, 1, "Controller look sensitivity");
        adsSensitivity = Dvar_RegisterFloat("gpad_ads_sensitivity", 0.45f, 0.1f, 1.0f,
                                            DVAR_ARCHIVE, "Controller sensitivity multiplier while aiming");
        aimAssist = Dvar_RegisterBool("gpad_aim_assist", true, DVAR_ARCHIVE,
                                     "Controller target slowdown and rotational aim assistance");
        invertPitch = Dvar_RegisterBool("gpad_invertPitch", false, 1, "Invert controller vertical look");
        // The console builds ship these four button and four stick layouts; the names are the
        // .cfg files in the game data (buttons_default_alt.cfg, thumbstick_southpaw.cfg, ...).
        buttonConfig = Dvar_RegisterString("gpad_buttonConfig", "buttons_default", DVAR_ARCHIVE,
                                           "Controller button layout: buttons_default, buttons_lefty or buttons_experimental");
        thumbstickConfig = Dvar_RegisterString("gpad_thumbStickConfig", "thumbstick_default", DVAR_ARCHIVE,
                                               "Stick layout: thumbstick_default, _southpaw, _legacy or _legacysouthpaw");
        gpadEnabled = Dvar_RegisterBool("gpad_enabled", true, DVAR_ARCHIVE, "Enable gamepad input");
        gpadRumble = Dvar_RegisterBool("gpad_rumble", true, DVAR_ARCHIVE, "Enable gamepad rumble");
        stickDeadzoneMin = Dvar_RegisterFloat("gpad_stick_deadzone_min", 0.2f, 0.0f, 1.0f, DVAR_ARCHIVE,
                                              "Stick deadzone at the centre");
        stickDeadzoneMax = Dvar_RegisterFloat("gpad_stick_deadzone_max", 0.01f, 0.0f, 1.0f, DVAR_ARCHIVE,
                                              "Stick deadzone at full deflection");
        buttonDeadzone = Dvar_RegisterFloat("gpad_button_deadzone", 0.13f, 0.0f, 1.0f, DVAR_ARCHIVE,
                                            "Trigger deadzone");
        stickPressed = Dvar_RegisterFloat("gpad_stick_pressed", 0.4f, 0.0f, 1.0f, DVAR_ARCHIVE,
                                          "Deflection at which a stick counts as a button press");
        lstickDeflectMax = Dvar_RegisterFloat("gpad_button_lstick_deflect_max", 1.0f, 0.0f, 1.0f, DVAR_ARCHIVE,
                                              "Left stick deflection treated as full");
        rstickDeflectMax = Dvar_RegisterFloat("gpad_button_rstick_deflect_max", 1.0f, 0.0f, 1.0f, DVAR_ARCHIVE,
                                              "Right stick deflection treated as full");
}
int KisakApple_ControllerTouchContext() { return touchContext.load(std::memory_order_relaxed); }

void KisakApple_ControllerFrame() {
    bool fullscreenMovie = R_Cinematic_IsStarted() && InMenu();
#ifdef KISAK_SP
    fullscreenMovie = R_Cinematic_IsStarted() && cg_cinematicFullscreen && cg_cinematicFullscreen->current.enabled;
#endif
    touchContext.store(fullscreenMovie ? 2 : (InMenu() ? 0 : 1), std::memory_order_relaxed);
    if (!sensitivity) {
        KisakApple_ControllerRegisterDvars();
    }
    static std::string activeButtons, activeSticks;
    if (FS_Initialized() && (!configLoaded || activeButtons != buttonConfig->current.string || activeSticks != thumbstickConfig->current.string)) {
        // Release old commands before their storage is overwritten by the new layout.
        const unsigned now = Sys_Milliseconds();
        const Edges edges = buttonState.Update(0, false, InMenu());
        for (int b = 0; b < Count; ++b) {
            if (edges.released & (1u << b)) Emit(heldRoutes[b], b, false, now);
            heldRoutes[b] = {};
        }
        repeatButton = -1;
        sprintRequested = false;
        activeButtons = buttonConfig->current.string;
        activeSticks = thumbstickConfig->current.string;
        LoadLayout();
    }
    std::deque<Snapshot> samples;
    { std::lock_guard<std::mutex> lock(inputMutex); samples.swap(pending); }
    const unsigned time = Sys_Milliseconds();
    for (const auto &sample : samples) {
        if (sample.connected != current.connected)
            Com_Printf(CON_CHANNEL_SYSTEM, "Controller %s\n", sample.connected ? "connected" : "disconnected: releasing inputs");
        if (sample.touch != current.touch || sample.connected != current.connected)
            touchLookX = touchLookY = 0;
        if (sample.touch && sample.connected && !InMenu()) {
            touchLookX += sample.lookDeltaX;
            touchLookY += sample.lookDeltaY;
        }
        current = sample;
        Process(time);
    }
    if (InMenu()) touchLookX = touchLookY = 0;
    Process(time); // Menu transitions and repeat also run without new samples.
    // Keep pointer motion separate from directional focus navigation.
    const unsigned elapsedMs = lastCursorTime ? time - lastCursorTime : 0;
    lastCursorTime = time;
    if (current.connected && (current.touch || !gpadEnabled || gpadEnabled->current.enabled) && InMenu() && pointerMenu) {
        float x = current.rightX, y = current.rightY;
        kisak::controller::Deadzone(x, y, StickDeadzone());
        int width = 0, height = 0;
        if ((x != 0.0f || y != 0.0f) && KisakApple_GetDisplaySize(&width, &height)) {
            const float pixelsPerSecond = 1.1f * static_cast<float>(height);
            const float seconds = std::min(elapsedMs, 100u) * 0.001f;
            KisakApple_ControllerCursor(x * pixelsPerSecond * seconds, -y * pixelsPerSecond * seconds, -1);
        }
    }
}
void KisakApple_ControllerMove(usercmd_s *cmd, float seconds) {
    if (!current.connected || InMenu()) return;
    if (!current.touch && gpadEnabled && !gpadEnabled->current.enabled) return; // Gamepad Enabled: No
    // Physical axes in the order thumbstick_*.cfg names them: A_LSTICK_X/Y, A_RSTICK_X/Y.
    float left[2] = {current.leftX, current.leftY};
    float right[2] = {current.rightX, current.rightY};
    kisak::controller::Deadzone(left[0], left[1], StickDeadzone(), stickDeadzoneMax ? stickDeadzoneMax->current.value : 0.01f);
    if (current.touch) {
        kisak::touch::SwipeAxes(touchLookX, touchLookY, std::clamp(seconds, .001f, .1f), right[0], right[1]);
        touchLookX = touchLookY = 0;
    } else {
    kisak::controller::Deadzone(right[0], right[1], StickDeadzone(), stickDeadzoneMax ? stickDeadzoneMax->current.value : 0.01f);
    }
    const float raw[4] = {left[0], left[1], right[0], right[1]};

    float side = 0.0f, forward = 0.0f, yaw = 0.0f, pitch = 0.0f;
    for (int axis = 0; axis < 4; ++axis) {
        const AxisBinding &binding = axisBindings[axis];
        const float value = current.touch && axis >= 2 ? raw[axis] : ApplyCurve(raw[axis], binding.squared);
        switch (binding.axis) {
        case VA_SIDE: side += value; break;
        case VA_FORWARD: forward += value; break;
        case VA_YAW: yaw += value; break;
        case VA_PITCH: pitch += value; break;
        default: break;
        }
    }
    if (axisBindings[0].axis == VA_NONE && axisBindings[1].axis == VA_NONE) {
        // No config loaded yet: the default layout (left stick moves, right stick looks).
        side = ApplyCurve(left[0], true);
        forward = ApplyCurve(left[1], true);
        yaw = right[0];
        pitch = right[1];
    }

    cmd->forwardmove = static_cast<int8_t>(std::clamp<int>(cmd->forwardmove + std::lround(forward * 127), -127, 127));
    cmd->rightmove = static_cast<int8_t>(std::clamp<int>(cmd->rightmove + std::lround(side * 127), -127, 127));
    if (sprintRequested) {
        cmd->buttons |= BUTTON_SPRINT;
        sprintRequested = false;
    }
    const playerState_s *ps = CG_GetPredictedPlayerState(0);
    if (!ps || (ps->pm_flags & PMF_FROZEN) != 0)
        return;
#ifdef KISAK_MP
    // Recorded/followed cameras use server angles. Keep local stick motion
    // from leaking into those views or the next respawn.
    if (ps->deltaTime || (ps->otherFlags & 2) != 0
        || CG_GetLocalClientGlobals(0)->inKillCam || ps->pm_type >= PM_DEAD)
        return;
#endif
    const float ads = std::clamp(ps->fWeaponPosFrac, 0.0f, 1.0f);
    const float adsScale = 1.0f + ads * ((adsSensitivity ? adsSensitivity->current.value : 0.45f) - 1.0f);
    // Match the engine's weapon FOV scaling so high-magnification scopes do
    // not retain the hip-fire turn rate.
    const float zoomScale = std::clamp(clients[0].cgameFOVSensitivityScale, 0.1f, 1.0f);
    const float lookScale = (sensitivity ? sensitivity->current.value : 1.0f) * adsScale * zoomScale;
    AimInput input{};
    input.localClientNum = 0;
    input.ps = ps;
    input.deltaTime = std::clamp(seconds, 0.0f, 0.1f);
    input.pitch = clients[0].viewangles[0];
    input.yaw = clients[0].viewangles[1];
    input.pitchAxis = pitch * (invertPitch && invertPitch->current.enabled ? 1.0f : -1.0f);
    input.yawAxis = -yaw;
    input.pitchMax = 120.0f * lookScale;
    input.yawMax = 180.0f * lookScale;
    input.forwardAxis = forward;
    input.rightAxis = side;
    input.buttons = cmd->buttons;
    AimOutput output{};
    AimAssist_UpdateGamepadInput(&input, &output, aimAssist && aimAssist->current.enabled);
    clients[0].viewangles[0] = output.pitch;
    clients[0].viewangles[1] = output.yaw;
    cmd->meleeChargeYaw = output.meleeChargeYaw;
    cmd->meleeChargeDist = output.meleeChargeDist;
}

bool KisakApple_ControllerBinding(const char *command, char *label, unsigned capacity) {
    if (!current.connected || (!current.touch && gpadEnabled && !gpadEnabled->current.enabled) || !command || !label || !capacity) return false;
    int button = -1;
    for (int b = 0; b < Count; ++b) if (commands[b] && !I_stricmp(command, commands[b])) button = b;
    struct Alias { const char *command; const char *action; };
    static const Alias aliases[] = {
        {"+activate", "+usereload"}, {"+reload", "+usereload"}, {"+use", "+usereload"}, {"+moveup", "+gostand"},
        {"+sprint", "+breath_sprint"}, {"+holdbreath", "+breath_sprint"}, {"+melee_breath", "+melee"},
        {"+prone", "+stance"}, {"toggleprone", "+stance"}, {"togglecrouch", "+stance"}, {"gocrouch", "+stance"}, {"goprone", "+stance"},
        {"toggleads", "+speed_throw"}, {"+toggleads_throw", "+speed_throw"}, {"+throw", "+frag"},
        {"weapprev", "weapnext"}, {"+nightvision", "+actionslot 1"}
    };
    // Resolve aliases through the selected layout so lefty/tactical prompts stay correct.
    if (button < 0) for (const auto &alias : aliases) if (!I_stricmp(command, alias.command))
        for (int b = 0; b < Count; ++b) if (commands[b] && !I_stricmp(alias.action, commands[b])) button = b;
    if (!I_stricmp(command, "pause") || !I_stricmp(command, "togglemenu")) button = Menu;
    if (button >= 0) {
        // Prefer the drawn Xbox 360 glyph; fall back to the controller's own label text.
        if (KisakApple_ControllerIconEscape(button, label, capacity))
            return true;
        std::snprintf(label, capacity, "[%s]", current.labels[button][0] ? current.labels[button] : fallbackLabels[button]);
        return true;
    }
    if (!I_stricmp(command, "+forward") || !I_stricmp(command, "+back") || !I_stricmp(command, "+moveleft") || !I_stricmp(command, "+moveright")) {
        std::snprintf(label, capacity, "[Left Stick]"); return true;
    }
    if (!I_stricmp(command, "+left") || !I_stricmp(command, "+right") || !I_stricmp(command, "+lookup") || !I_stricmp(command, "+lookdown")) {
        std::snprintf(label, capacity, "[Right Stick]"); return true;
    }
    return false;
}

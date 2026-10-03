#include <TargetConditionals.h>
#include <execinfo.h>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
// Apple replacement for the parts of src/win32 the single-player engine calls:
// process lifetime, the system event queue, system information, and the
// startup sequence and frame loop from WinMain. The window, console, remote
// script debugger and mouse code have no Apple counterpart in a headless run;
// those entry points report the feature as absent instead of pretending to work.

#include <universal/q_shared.h>
#include <win32/win_local.h>
#include <win32/win_localize.h>
#include <win32/win_net_debug.h>
#include <win32/win_input.h>

#include <client/client.h> // shared client declarations (it branches internally on SP/MP)
#ifdef KISAK_MP
#include <client_mp/client_mp.h>
#else
#include <client/cl_input.h>
#endif
#include <ui/keycodes.h>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include "controller_input.h"

#include <qcommon/qcommon.h>
#include <qcommon/cmd.h>
#include <qcommon/threads.h>
#include <qcommon/mem_track.h>

#include <universal/com_memory.h>
#include <universal/q_parse.h>
#include <universal/timing.h>
#include <universal/profile.h>

#include <sys/sysctl.h>
#include <unistd.h>
#include <cstdarg>

char sys_cmdline[1024];

sysEvent_t eventQue[MAX_QUED_EVENTS];
int eventHead;
int eventTail;

SysInfo sys_info;
int client_state;
HWND g_splashWnd;
WinVars_t g_wv;

cmd_function_s Sys_In_Restart_f_VAR;

// ---------------------------------------------------------------------------
// Process lifetime

void Sys_Error(const char *error, ...)
{
    char string[4100];
    va_list va;

    va_start(va, error);
    Sys_EnterCriticalSection(CRITSECT_COM_ERROR);
    Com_PrintStackTrace();
    com_errorEntered = 1;
    Sys_SuspendOtherThreads();
    vsnprintf(string, sizeof(string), error, va);
    va_end(va);

    fprintf(stderr, "Sys_Error: %s\n", string);
    {
        void *frames[64];
        const int frameCount = backtrace(frames, 64);
        fprintf(stderr, "Sys_Error backtrace (%d frames):\n", frameCount);
        fflush(stderr);
        backtrace_symbols_fd(frames, frameCount, fileno(stderr));
    }
    fflush(stderr);
    exit(1);
}

void __cdecl Sys_OutOfMemErrorInternal(const char *filename, int line)
{
    Sys_EnterCriticalSection(CRITSECT_FATAL_ERROR);
    Com_Printf(CON_CHANNEL_SYSTEM, "Out of memory: filename '%s', line %d\n", filename, line);
    fprintf(stderr, "Out of memory: filename '%s', line %d\n", filename, line);
    exit(-1);
}

void __cdecl Sys_NormalExit()
{
    // Windows removes its crash-detection semaphore file here; Apple builds
    // do not create one.
}

void __cdecl Sys_Quit()
{
    // iOS apps normally never quit themselves; record who asked so unexpected exits are diagnosable.
    {
        fprintf(stderr, "Sys_Quit: engine requested process exit (com_errorEntered %d)\n", (int)com_errorEntered);
        void *frames[32];
        const int frameCount = backtrace(frames, 32);
        backtrace_symbols_fd(frames, frameCount, fileno(stderr));
        fflush(stderr);
    }
    Sys_EnterCriticalSection(CRITSECT_COM_ERROR);
    IN_Shutdown();
    Key_Shutdown();
    Sys_NormalExit();
    Win_ShutdownLocalization();
    RefreshQuitOnErrorCondition();
    Dvar_Shutdown();
    Cmd_Shutdown();
    Sys_ShutdownEvents();
    SL_Shutdown();
    if (!com_errorEntered)
        track_shutdown(0);
    Con_ShutdownChannels();
    exit(0);
}

void __cdecl Sys_Print(const char *msg)
{
    // Strip ^N colour codes the way the Windows console buffer does.
    char cleaned[4096];
    size_t out = 0;
    for (const char *src = msg; *src && out + 1 < sizeof(cleaned); ++src)
    {
        if (src[0] == '^' && src[1] >= '0' && src[1] <= '9')
        {
            ++src;
            continue;
        }
        cleaned[out++] = *src;
    }
    cleaned[out] = 0;
    fputs(cleaned, stdout);
    fflush(stdout);
}

void __cdecl Sys_OpenURL(const char *url, int doexit)
{
    Com_Printf(CON_CHANNEL_SYSTEM, "Open URL: %s\n", url);
    if (doexit)
        Cbuf_AddText(0, "quit\n");
}

char *__cdecl Sys_GetClipboardData()
{
    // No clipboard integration yet.
    return nullptr;
}

int __cdecl Sys_SetClipboardData(const char *text)
{
    (void)text;
    return 0;
}

// ---------------------------------------------------------------------------
// System event queue (same ring buffer as win_main.cpp, without a message pump)

void __cdecl Sys_QueEvent(uint32_t time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr)
{
    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    sysEvent_t *ev = &eventQue[(unsigned __int8)eventHead];
    if (eventHead - eventTail >= MAX_QUED_EVENTS)
    {
        Com_Printf(CON_CHANNEL_SYSTEM, "Sys_QueEvent: overflow\n");
        if (ev->evPtr)
            Z_Free((char *)ev->evPtr, 10);
        ++eventTail;
    }
    ++eventHead;
    if (!time)
        time = Sys_Milliseconds();
    ev->evTime = time;
    ev->evType = type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
}

void Sys_ShutdownEvents()
{
    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    while (eventHead > eventTail)
    {
        sysEvent_t *ev = &eventQue[(unsigned __int8)eventTail++];
        if (ev->evPtr)
            Z_Free((char *)ev->evPtr, 10);
    }
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
}

sysEvent_t *__cdecl Sys_GetEvent(sysEvent_t *result)
{
    PROF_SCOPED("Sys_GetEvent");

    sysEvent_t ev;
    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    if (eventHead > eventTail)
    {
        ev = eventQue[(unsigned __int8)eventTail++];
    }
    else
    {
        memset(&ev, 0, sizeof(ev));
        ev.evTime = Sys_Milliseconds();
    }
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    *result = ev;
    return result;
}

void __cdecl Sys_LoadingKeepAlive()
{
    sysEvent_t ev;
    do
    {
        Sys_GetEvent(&ev);
    } while (ev.evType);
    // Windows also recovers a lost D3D device here; that belongs to the
    // Apple renderer once it exists.
}

// ---------------------------------------------------------------------------
// System information

static void Sys_ReadSysctlString(const char *name, char *buffer, size_t size, const char *fallback)
{
    size_t length = size;
    if (sysctlbyname(name, buffer, &length, nullptr, 0) != 0 || !buffer[0])
        I_strncpyz(buffer, fallback, size);
    buffer[size - 1] = 0;
}

static int Sys_ReadSysctlInt(const char *name, int fallback)
{
    int value = 0;
    size_t length = sizeof(value);
    return sysctlbyname(name, &value, &length, nullptr, 0) == 0 && value > 0 ? value : fallback;
}

static int Sys_SystemMemoryMB()
{
    uint64_t bytes = 0;
    size_t length = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &length, nullptr, 0) != 0)
        return 1024;
    const uint64_t megabytes = bytes / (1024 * 1024);
    // Same cap the Windows build applies.
    return megabytes > 1024 ? 1024 : static_cast<int>(megabytes);
}

static void Sys_FindInfo()
{
    sys_info.logicalCpuCount = Sys_GetCpuCount();
    sys_info.physicalCpuCount = Sys_ReadSysctlInt("hw.physicalcpu", sys_info.logicalCpuCount);
    // The Windows build benchmarks x86 cores to pick default detail settings.
    // Apple arm64 cores comfortably exceed that range, so report a fixed rating.
    sys_info.cpuGHz = 3.0;
    sys_info.configureGHz = sys_info.cpuGHz * (sys_info.physicalCpuCount > 2 ? 2 : sys_info.physicalCpuCount);
    sys_info.sysMB = Sys_SystemMemoryMB();
    I_strncpyz(sys_info.gpuDescription, "Apple GPU", sizeof(sys_info.gpuDescription));
    // No x86 SIMD paths exist on arm64.
    sys_info.SSE = 0;
    I_strncpyz(sys_info.cpuVendor, "Apple", sizeof(sys_info.cpuVendor));
    Sys_ReadSysctlString("machdep.cpu.brand_string", sys_info.cpuName, sizeof(sys_info.cpuName), "Apple arm64");
}

void Sys_In_Restart_f()
{
    IN_Shutdown();
    IN_Init();
}

void __cdecl Sys_Init()
{
    Cmd_AddCommandInternal("in_restart", Sys_In_Restart_f, &Sys_In_Restart_f_VAR);

    Com_Printf(CON_CHANNEL_SYSTEM, "CPU vendor is \"%s\"\n", sys_info.cpuVendor);
    Com_Printf(CON_CHANNEL_SYSTEM, "CPU name is \"%s\"\n", sys_info.cpuName);
    Com_Printf(CON_CHANNEL_SYSTEM, "%i logical CPUs reported\n", sys_info.logicalCpuCount);
    Com_Printf(CON_CHANNEL_SYSTEM, "%i physical CPUs detected\n", sys_info.physicalCpuCount);
    Com_Printf(CON_CHANNEL_SYSTEM, "System memory is %i MB (capped at 1 GB)\n", sys_info.sysMB);
    Com_Printf(CON_CHANNEL_SYSTEM, "Video card is \"%s\"\n", sys_info.gpuDescription);
    Com_Printf(CON_CHANNEL_SYSTEM, "\n");
    IN_Init();
}

// ---------------------------------------------------------------------------
// Networking: single player only needs the sleep primitive.

void NET_Sleep(int msec)
{
    if (msec > 0)
        usleep(static_cast<useconds_t>(msec) * 1000);
}

void __cdecl NET_Init()
{
#ifdef KISAK_MP
    // Multiplayer needs real sockets (apple_net.cpp); single player never sends a packet.
    // net_port/net_serverport are the same dvars the Windows build binds to.
    const dvar_t *clientPort = Dvar_RegisterInt("net_port", 28960, 0, 0xFFFF, DVAR_LATCH, "Network port");
    const dvar_t *serverPort = Dvar_RegisterInt("net_serverPort", 28960, 0, 0xFFFF, DVAR_LATCH, "Server network port");
    extern void Sys_InitNetworking(uint16_t clientPort, uint16_t serverPort);
    // The client binds an ephemeral port so it can run alongside a server on 28960.
    Sys_InitNetworking(static_cast<uint16_t>(clientPort->current.integer + 1),
                       static_cast<uint16_t>(serverPort->current.integer));
#endif
}

// ---------------------------------------------------------------------------
// Remote script debugger (a Windows development tool): never connected.

int g_debugClient;
unsigned __int8 g_debugPacket[1][8192];

int __cdecl Sys_IsRemoteDebugClient() { return 0; }
void __cdecl NET_ShutdownDebug() {}
void NET_InitDebug() {}
void NET_RestartDebug() {}
void __cdecl Sys_Listen_f() {}
void Sys_DebugSocketError(const char *message) { (void)message; }
int __cdecl Sys_ReadDebugSocketInt() { return 0; }
void __cdecl Sys_WriteDebugSocketInt(int value) { (void)value; }
void __cdecl Sys_WriteDebugSocketString(char *text) { (void)text; }
int __cdecl Sys_ReadDebugSocketMessageType(unsigned __int8 *type, int blocking) { (void)type; (void)blocking; return 0; }
int __cdecl Sys_UpdateDebugSocket() { return 0; }
int __cdecl Sys_ReadDebugSocketData(char *buffer, int len, int blocking) { (void)buffer; (void)len; (void)blocking; return 0; }
void __cdecl Sys_ReadDebugSocketStringBuffer(char *buffer, int len) { if (len > 0) buffer[0] = 0; }
void __cdecl Sys_FlushDebugSocketData() {}
void __cdecl Sys_AckDebugSocket() {}
char *__cdecl Sys_ReadDebugSocketString() { static char empty[1]; return empty; }
void __cdecl Sys_WriteDebugSocketData(unsigned __int8 *buffer, int len) { (void)buffer; (void)len; }
void __cdecl Sys_WriteDebugSocketMessageType(unsigned __int8 type) { (void)type; }
void __cdecl Sys_EndWriteDebugSocket() {}

// ---------------------------------------------------------------------------
// Input: a headless run has no window or mouse. Touch and controller input
// arrive through the iOS app layer.

void IN_Init() {}
void IN_Shutdown() {}
// Touch input from the launcher view (UI thread), replayed on the engine thread.
namespace {
struct KisakTouch
{
    int phase;
    int x;
    int y;
};
std::mutex g_touchLock;
std::vector<KisakTouch> g_touches;
int g_cursorX = 0;
int g_cursorY = 0;
bool g_haveCursor = false;
} // namespace

extern int g_editingField;

int KisakApple_TextInputActive()
{
    return g_editingField;
}

// Keyboard input from the launcher (UI thread). Sys_QueEvent takes the event
// queue lock, so these are safe to call off the engine thread.
void KisakApple_TextInput(const char *utf8)
{
    if (!utf8)
        return;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(utf8); *c; ++c)
    {
        if (*c >= 32 && *c < 127)
            Sys_QueEvent(0, SE_CHAR, *c, 0, 0, nullptr);
    }
}

void KisakApple_TextBackspace()
{
    Sys_QueEvent(0, SE_KEY, K_BACKSPACE, 1, 0, nullptr);
    Sys_QueEvent(0, SE_KEY, K_BACKSPACE, 0, 0, nullptr);
}

void KisakApple_TextReturn()
{
    Sys_QueEvent(0, SE_KEY, K_ENTER, 1, 0, nullptr);
    Sys_QueEvent(0, SE_KEY, K_ENTER, 0, 0, nullptr);
}

void KisakApple_TouchEvent(int phase, float x, float y)
{
    std::lock_guard<std::mutex> guard(g_touchLock);
    g_touches.push_back(KisakTouch{phase, static_cast<int>(x), static_cast<int>(y)});
}

void IN_Frame()
{
    KisakApple_ControllerFrame();
    std::vector<KisakTouch> touches;
    {
        std::lock_guard<std::mutex> guard(g_touchLock);
        touches.swap(g_touches);
    }
    for (const KisakTouch &touch : touches)
    {
        const int dx = g_haveCursor ? touch.x - g_cursorX : 0;
        const int dy = g_haveCursor ? touch.y - g_cursorY : 0;
        g_cursorX = touch.x;
        g_cursorY = touch.y;
        g_haveCursor = true;
        // Move first so a tap clicks where the finger landed.
        CL_MouseEvent(touch.x, touch.y, dx, dy);
        if (touch.phase == 0)
            Sys_QueEvent(0, SE_KEY, K_MOUSE1, 1, 0, nullptr);
        else if (touch.phase == 2)
            Sys_QueEvent(0, SE_KEY, K_MOUSE1, 0, 0, nullptr);
    }
}
void __cdecl IN_ShowSystemCursor(BOOL show) { (void)show; }
void __cdecl IN_SetForegroundWindow() {}
bool __cdecl IN_IsForegroundWindow() { return true; }
void IN_ActivateMouse(qboolean force) { (void)force; }

// ---------------------------------------------------------------------------
// Render view and display, supplied by the app before the engine starts.

static void *s_renderWindow;
static int s_displayWidth;
static int s_displayHeight;
static float s_displaySafeHorizontal, s_displaySafeVertical;

void KisakApple_SetDisplaySafeArea(float horizontal, float vertical)
{
    s_displaySafeHorizontal = horizontal;
    s_displaySafeVertical = vertical;
}

void KisakApple_GetDisplaySafeArea(float *horizontal, float *vertical)
{
    *horizontal = s_displaySafeHorizontal;
    *vertical = s_displaySafeVertical;
}


void KisakApple_SetRenderWindow(void *view, int width, int height)
{
    s_renderWindow = view;
    s_displayWidth = width;
    s_displayHeight = height;
}

HWND KisakApple_GetRenderWindow()
{
    return static_cast<HWND>(s_renderWindow);
}

bool KisakApple_GetDisplaySize(int *width, int *height)
{
    *width = s_displayWidth;
    *height = s_displayHeight;
    return s_displayWidth > 0 && s_displayHeight > 0;
}

// Controller-driven menu cursor: moves the cursor that touches use (render-target pixels) and optionally presses or
// releases the left mouse button (button: 1 down, 0 up, -1 none). Called from IN_Frame via KisakApple_ControllerFrame.
void KisakApple_ControllerCursor(float dx, float dy, int button)
{
    if (!g_haveCursor)
    {
        g_cursorX = s_displayWidth / 2;
        g_cursorY = s_displayHeight / 2;
        g_haveCursor = true;
    }
    const int maxX = s_displayWidth > 0 ? s_displayWidth - 1 : 0;
    const int maxY = s_displayHeight > 0 ? s_displayHeight - 1 : 0;
    const int x = std::clamp(g_cursorX + static_cast<int>(std::lround(dx)), 0, maxX);
    const int y = std::clamp(g_cursorY + static_cast<int>(std::lround(dy)), 0, maxY);
    const int moveX = x - g_cursorX;
    const int moveY = y - g_cursorY;
    g_cursorX = x;
    g_cursorY = y;
    if (moveX || moveY || button >= 0)
        CL_MouseEvent(x, y, moveX, moveY);
    if (button >= 0)
        Sys_QueEvent(0, SE_KEY, K_MOUSE1, button, 0, nullptr);
}

// ---------------------------------------------------------------------------
// Startup sequence and frame loop from WinMain.

static void KisakApple_ProbeGameData()
{
    const char *root = getenv("KISAK_INSTALL_PATH");
    if (!root)
        return;
    const char *rels[] = { "main/iw_00.iwd", "zone", "zone/italian", "zone/italian/code_post_gfx.ff", "localization.txt" };
    for (const char *rel : rels)
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", root, rel);
        struct stat linkInfo, info;
        const int l = lstat(path, &linkInfo);
        const int st = stat(path, &info);
        const int statErr = st ? errno : 0;
        const int fd = open(path, O_RDONLY | O_CLOEXEC);
        const int openErr = fd < 0 ? errno : 0;
        if (fd >= 0)
            close(fd);
        char resolved[1024] = "";
        const char *rp = realpath(path, resolved);
        fprintf(stderr, "probe %s: lstat=%d link=%d stat=%d(%s) open=%s(%s) real=%s\n", rel, l, l == 0 && S_ISLNK(linkInfo.st_mode),
                st, strerror(statErr), fd >= 0 ? "ok" : "fail", strerror(openErr), rp ? resolved : "-");
    }
}

int KisakApple_RunEngine(const char *commandLine)
{
    KisakApple_ProbeGameData();
    Sys_InitializeCriticalSections();
    Sys_InitMainThread();
    track_init();
    Win_InitLocalization();

    Com_InitParse();
    Dvar_Init();
    InitTiming();
    Sys_FindInfo();
    I_strncpyz(sys_cmdline, commandLine ? commandLine : "", sizeof(sys_cmdline));
    Sys_Milliseconds();
    Profile_Init();
    Profile_InitContext(0);
    Com_Init(sys_cmdline);
    // Explicitly uncapped on iOS, including profiles that archived a 60/85 cap.
    Dvar_SetInt(com_maxfps, 0);

#ifdef KISAK_MP
    // LiveStorage_Init (which registers readStats) only runs in multiplayer builds.
    Cbuf_AddText(0, "readStats\n");
#endif
    Com_Printf(CON_CHANNEL_SYSTEM, "Working directory: %s\n", Sys_Cwd());

#if 1 // was TARGET_OS_SIMULATOR: the fixture is wanted on hardware too
    // Test fixture: KISAK_TEST_COMMANDS="30:give m4_grunt;20:weapnext" runs each command once, the given number of
    // seconds after the previous one (starting when the engine loop begins). Inert unless the
    // variable is set, and available on device too so a join can be driven from the host:
    //   DEVICECTL_CHILD_KISAK_TEST_COMMANDS="25:connect 1.2.3.4:28960" xcrun devicectl device process launch ...
    const char *testCommands = getenv("KISAK_TEST_COMMANDS");
    std::vector<std::pair<int, std::string>> pendingTests;
    if (testCommands)
    {
        std::string all(testCommands);
        size_t start = 0;
        while (start < all.size())
        {
            size_t end = all.find(';', start);
            std::string entry = all.substr(start, end == std::string::npos ? std::string::npos : end - start);
            size_t colon = entry.find(':');
            if (colon != std::string::npos)
                pendingTests.emplace_back(atoi(entry.substr(0, colon).c_str()) * 1000, entry.substr(colon + 1));
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    }
    int nextTestTime = pendingTests.empty() ? 0 : Sys_Milliseconds() + pendingTests.front().first;
    size_t nextTest = 0;
#endif
    while (1)
    {
        Com_Frame();
#if 1 // was TARGET_OS_SIMULATOR: the fixture is wanted on hardware too
        if (nextTest < pendingTests.size() && Sys_Milliseconds() >= nextTestTime)
        {
            Com_Printf(CON_CHANNEL_SYSTEM, "Test command: %s\n", pendingTests[nextTest].second.c_str());
            Cbuf_AddText(0, (pendingTests[nextTest].second + "\n").c_str());
            if (++nextTest < pendingTests.size())
                nextTestTime = Sys_Milliseconds() + pendingTests[nextTest].first;
        }
#endif
    }

    return 0;
}

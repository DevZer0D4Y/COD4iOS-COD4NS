// iPhone launcher for the single-player engine.
//
// The game data comes from the user's own copy of Call of Duty 4: the contents
// of the game folder (main/, zone/, localization.txt) are copied into this
// app's Documents folder through Finder or the Files app. The engine runs on
// its own thread with a large stack, draws into the Metal-backed view once the
// renderer backend exists, and writes its console to Documents/kisakcod.log.

#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CADisplayLink.h>
#import <GameController/GameController.h>
#include "../engine/controller_input.h"
#import "touch_controls.h"
#import "client_patch.h"

// Com_Printf, so controller diagnostics land in the console log (stderr is not captured here).
void Com_Printf(int channel, const char *format, ...); // C++ linkage, as declared in qcommon.h
#define KISAK_CON_CHANNEL_SYSTEM 0

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach.h>

int KisakApple_RunEngine(const char *commandLine);
void KisakApple_SetRenderWindow(void *view, int width, int height);
void KisakApple_SetDisplaySafeArea(float horizontal, float vertical);
void KisakApple_TouchEvent(int phase, float x, float y);
void KisakApple_TextInput(const char *utf8);
void KisakApple_TextBackspace();
void KisakApple_TextReturn();
int KisakApple_TextInputActive();

@interface KISControllerInput : NSObject
- (void)start;
- (void)publish;
@property(nonatomic,strong) KISTouchControls *touchControls;
@end

@implementation KISControllerInput {
    GCController *_controller;
    BOOL _active;
    NSTimer *_pollTimer;
}
- (void)publish
{
    using namespace kisak::controller;
    Snapshot sample;
    [self.touchControls setAvailable:_active && !_controller && !KisakApple_TextInputActive()
                            context:KisakApple_ControllerTouchContext()];
    if (_active && !_controller) sample=[self.touchControls sample];
    GCExtendedGamepad *pad = _controller.extendedGamepad;
    if (_active && pad) {
        sample.connected = true;
        sample.leftX = pad.leftThumbstick.xAxis.value;
        sample.leftY = pad.leftThumbstick.yAxis.value;
        sample.rightX = pad.rightThumbstick.xAxis.value;
        sample.rightY = pad.rightThumbstick.yAxis.value;
        sample.leftTrigger = pad.leftTrigger.value;
        sample.rightTrigger = pad.rightTrigger.value;
        GCControllerButtonInput *buttons[Count] = {
            pad.buttonA, pad.buttonB, pad.buttonX, pad.buttonY,
            pad.leftShoulder, pad.rightShoulder, pad.leftTrigger, pad.rightTrigger,
            pad.leftThumbstickButton, pad.rightThumbstickButton, pad.buttonMenu, pad.buttonOptions,
            pad.dpad.up, pad.dpad.down, pad.dpad.left, pad.dpad.right
        };
        GCControllerButtonInput *share = _controller.physicalInputProfile.buttons[GCInputButtonShare];
        if (share.isPressed) sample.buttons |= 1u << Options;
        if (!buttons[Options]) buttons[Options] = share;
        static BOOL loggedElements = NO;
        if (!loggedElements) {
            loggedElements = YES;
            // Not every pad reports stick clicks; melee sits on the right stick in the stock
            // Xbox 360 layout, so record what this controller actually exposes. This goes to the
            // console log rather than stderr - stderr is not captured on the simulator, so the one
            // fact needed to tell "pad cannot report R3" from "our mapping is wrong" was invisible.
            Com_Printf(KISAK_CON_CHANNEL_SYSTEM,
                       "GameController: %s exposes A=%s B=%s X=%s Y=%s LB=%s RB=%s LT=%s RT=%s L3=%s R3=%s Start=%s Back=%s\n",
                       _controller.vendorName.UTF8String ?: "pad",
                       pad.buttonA ? "y" : "n", pad.buttonB ? "y" : "n",
                       pad.buttonX ? "y" : "n", pad.buttonY ? "y" : "n",
                       pad.leftShoulder ? "y" : "n", pad.rightShoulder ? "y" : "n",
                       pad.leftTrigger ? "y" : "n", pad.rightTrigger ? "y" : "n",
                       pad.leftThumbstickButton ? "y" : "n", pad.rightThumbstickButton ? "y" : "n",
                       pad.buttonMenu ? "y" : "n", pad.buttonOptions ? "y" : "n");
        }
        {
            // Raw stick-click state, straight from the element: tells a pad that never reports the
            // click apart from a mapping problem on our side.
            static BOOL lastL3 = NO, lastR3 = NO;
            const BOOL l3 = pad.leftThumbstickButton.isPressed, r3 = pad.rightThumbstickButton.isPressed;
            if (l3 != lastL3 || r3 != lastR3) {
                lastL3 = l3; lastR3 = r3;
                Com_Printf(KISAK_CON_CHANNEL_SYSTEM, "GameController: raw stick click L3=%d R3=%d\n", (int)l3, (int)r3);
            }
        }
        for (int b = 0; b < Count; ++b) {
            if (buttons[b].isPressed) sample.buttons |= 1u << b;
            // localizedName follows the controller's physical layout AND any
            // remapping configured by the player in iOS Settings.
            NSString *name = buttons[b].localizedName;
            NSString *symbol = buttons[b].sfSymbolsName;
            NSDictionary<NSString *, NSString *> *shortNames = @{
                @"a.circle": @"A", @"b.circle": @"B", @"x.circle": @"X", @"y.circle": @"Y",
                @"xmark.circle": @"Cross", @"square.circle": @"Square",
                @"circle.circle": @"Circle", @"triangle.circle": @"Triangle"
            };
            if (symbol && shortNames[symbol]) name = shortNames[symbol];
            // The original font/localization renderer expects Windows-1252.
            NSData *encoded = [name dataUsingEncoding:NSWindowsCP1252StringEncoding allowLossyConversion:YES];
            if (encoded.length) {
                const size_t count = MIN(encoded.length, sizeof(sample.labels[b]) - 1);
                memcpy(sample.labels[b], encoded.bytes, count);
                for (size_t i = 0; i < count; ++i)
                    if (static_cast<unsigned char>(sample.labels[b][i]) < 32 || sample.labels[b][i] == '^') sample.labels[b][i] = ' ';
            }
        }
    }
    KisakApple_ControllerSubmit(sample);
}
- (void)refresh:(NSNotification *)notification
{
    (void)notification;
    GCController *selected = nil;
    // Keep the active controller when a second one connects.
    if (_controller && [GCController.controllers containsObject:_controller]) selected = _controller;
    if (!selected)
        for (GCController *candidate in GCController.controllers)
            if (candidate.extendedGamepad) { selected = candidate; break; }
#if TARGET_OS_SIMULATOR
    // Explicit test fixture, never enabled in a device build.
    if (!selected && getenv("KISAK_CONTROLLER_TEST")) {
        selected = _controller.isSnapshot ? _controller : [GCController controllerWithExtendedGamepad];
        selected.extendedGamepad.buttonX.localizedName = @"X";
        selected.extendedGamepad.buttonA.localizedName = @"A";
    }
#endif
    static NSUInteger lastCount = NSUIntegerMax;
    if (GCController.controllers.count != lastCount) {
        lastCount = GCController.controllers.count;
        fprintf(stderr, "GameController: %lu controller(s) visible\n", static_cast<unsigned long>(lastCount));
        for (GCController *candidate in GCController.controllers)
            fprintf(stderr, "GameController:   %s (%s)\n", candidate.vendorName.UTF8String ?: "unnamed",
                    candidate.extendedGamepad ? "extended gamepad" : "no extended gamepad profile");
    }
    if (selected != _controller) {
        _controller.extendedGamepad.valueChangedHandler = nil;
        [self.touchControls cancelInputs];
        KisakApple_ControllerSubmit({});
        _controller = selected;
        _controller.handlerQueue = dispatch_get_main_queue();
        // Xbox Series Share is reserved for screenshots by default. In game,
        // use it for the same scoreboard action as Xbox 360 Back / Select.
        _controller.physicalInputProfile.buttons[GCInputButtonShare].preferredSystemGestureState = GCSystemGestureStateDisabled;
        __weak KISControllerInput *weakSelf = self;
        _controller.extendedGamepad.valueChangedHandler = ^(GCExtendedGamepad *pad, GCControllerElement *element) {
            (void)pad; (void)element;
            [weakSelf publish];
        };
        fprintf(stderr, "GameController: %s\n", _controller ? (_controller.vendorName.UTF8String ?: "extended gamepad") : "none connected");
    }
    [self publish];
}
- (void)resign:(NSNotification *)notification
{
    (void)notification;
    _active = NO;
    [self.touchControls setAvailable:NO context:0];
    KisakApple_ControllerSubmit({});
}
- (void)activate:(NSNotification *)notification
{
    _active = YES;
    [self refresh:notification];
}
- (void)start
{
    _active = UIApplication.sharedApplication.applicationState == UIApplicationStateActive;
    // Poll as well as listen: some controllers never fire valueChangedHandler for the thumbstick
    // buttons (melee sits on the right stick), so a handler-only snapshot would miss those presses.
    _pollTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0 repeats:YES block:^(NSTimer *timer) {
        (void)timer;
        [self publish];
    }];
    [NSRunLoop.mainRunLoop addTimer:_pollTimer forMode:NSRunLoopCommonModes];
    _pollTimer.tolerance = 1.0 / 240.0;
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(refresh:) name:GCControllerDidConnectNotification object:nil];
    [center addObserver:self selector:@selector(refresh:) name:GCControllerDidDisconnectNotification object:nil];
    [center addObserver:self selector:@selector(refresh:) name:GCControllerDidBecomeCurrentNotification object:nil];
    [center addObserver:self selector:@selector(resign:) name:UIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(activate:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [self refresh:nil];
}
- (void)dealloc
{
    [_pollTimer invalidate];
    [NSNotificationCenter.defaultCenter removeObserver:self];
    _controller.extendedGamepad.valueChangedHandler = nil;
    KisakApple_ControllerSubmit({});
}
@end

@interface KISEngineView : UIView <UIKeyInput>
// YES while the on-screen keyboard should be visible. The view stays first
// responder either way so a hardware keyboard always reaches the game; an
// empty input view hides the on-screen keyboard when no text field is active.
@property(nonatomic) BOOL wantsKeyboard;
@end

@implementation KISEngineView
+ (Class)layerClass { return CAMetalLayer.class; }

// One finger drives the menu cursor: the engine sees touches as mouse moves in
// render-target pixels, and touch down/up as the left mouse button.
- (void)forwardTouches:(NSSet<UITouch *> *)touches phase:(int)phase
{
    UITouch *touch = touches.anyObject;
    if (!touch)
        return;
    const CGPoint point = [touch locationInView:self];
    if (phase != 1)
        fprintf(stderr, "Touch: phase=%d point=%.1f,%.1f\n", phase, point.x, point.y);
    const CGSize pixels = ((CAMetalLayer *)self.layer).drawableSize;
    const CGSize bounds = self.bounds.size;
    if (bounds.width <= 0 || bounds.height <= 0)
        return;
    KisakApple_TouchEvent(phase, static_cast<float>(point.x * pixels.width / bounds.width),
                         static_cast<float>(point.y * pixels.height / bounds.height));
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)event; [self forwardTouches:touches phase:0]; }
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)event; [self forwardTouches:touches phase:1]; }
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)event; [self forwardTouches:touches phase:2]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { (void)event; [self forwardTouches:touches phase:2]; }

// On-screen keyboard: the view becomes first responder while a game text field
// (profile name, save name, ...) is being edited; see -syncKeyboard.
- (BOOL)canBecomeFirstResponder { return YES; }

- (UIView *)inputView
{
    static UIView *hidden = nil;
    if (!hidden)
        hidden = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, 0)];
    return self.wantsKeyboard ? nil : hidden;
}

- (void)setWantsKeyboard:(BOOL)wantsKeyboard
{
    if (_wantsKeyboard == wantsKeyboard)
        return;
    _wantsKeyboard = wantsKeyboard;
    if (!self.isFirstResponder)
        [self becomeFirstResponder];
    [self reloadInputViews];
}
- (BOOL)hasText { return YES; }
- (void)insertText:(NSString *)text
{
    if ([text isEqualToString:@"\n"])
        KisakApple_TextReturn();
    else
        KisakApple_TextInput(text.UTF8String);
}
- (void)deleteBackward { KisakApple_TextBackspace(); }
- (UITextAutocorrectionType)autocorrectionType { return UITextAutocorrectionTypeNo; }
- (UITextAutocapitalizationType)autocapitalizationType { return UITextAutocapitalizationTypeNone; }
- (UITextSpellCheckingType)spellCheckingType { return UITextSpellCheckingTypeNo; }
- (UIKeyboardType)keyboardType { return UIKeyboardTypeASCIICapable; }
- (UIReturnKeyType)returnKeyType { return UIReturnKeyDone; }
@end

static void *KISEngineThreadMain(void *argument)
{
    (void)argument;
    // Development aid: engine startup commands (for example "+devmap killhouse")
    // come from KISAK_COMMANDLINE, which simctl forwards as SIMCTL_CHILD_KISAK_COMMANDLINE.
    const char *commandLine = getenv("KISAK_COMMANDLINE");
    KisakApple_RunEngine(commandLine ? commandLine : "");
    return nullptr;
}

@interface KISEngineViewController : UIViewController <UIGestureRecognizerDelegate>
@end

@implementation KISEngineViewController {
    KISEngineView *_engineView;
    UILabel *_status;
    BOOL _started;
    BOOL _preparingPatch, _patchReady;
    BOOL _engineEditing;
    KISControllerInput *_controllerInput;
    KISTouchControls *_touchControls;
    CADisplayLink *_displayLink;
}

- (void)loadView
{
    _engineView = [KISEngineView new];
    _engineView.backgroundColor = UIColor.blackColor;
    self.view = _engineView;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    _status = [UILabel new];
    _status.textColor = UIColor.whiteColor;
    _status.numberOfLines = 0;
    _status.font = [UIFont systemFontOfSize:15 weight:UIFontWeightMedium];
    _status.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_status];
    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [_status.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:24],
        [_status.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-24],
        [_status.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor],
    ]];
}

- (NSString *)documentsPath
{
    return NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
}

- (BOOL)hasGameData:(NSString *)root
{
    NSFileManager *files = NSFileManager.defaultManager;
    BOOL isDirectory = NO;
    return [files fileExistsAtPath:[root stringByAppendingPathComponent:@"localization.txt"]]
        && [files fileExistsAtPath:[root stringByAppendingPathComponent:@"main"] isDirectory:&isDirectory] && isDirectory
        && [files fileExistsAtPath:[root stringByAppendingPathComponent:@"zone"] isDirectory:&isDirectory] && isDirectory;
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    [self startIfReady];
}

- (void)startIfReady
{
    if (_started)
        return;

    NSString *root = [self documentsPath];
    if (![self hasGameData:root])
    {
        _status.text = @"Game files not found.\n\nConnect the iPhone to your Mac, open it in Finder, choose Files → COD4iOS, "
                       @"and copy in the contents of your Call of Duty 4 folder: localization.txt, main and zone. "
                       @"Then reopen the app.";
        return;
    }

#ifdef KISAK_MP
    if(!_patchReady) {
        if(_preparingPatch)return;
        _preparingPatch=YES;
        _status.text=@"Preparing CoD4x multiplayer…";
        __weak KISEngineViewController *weakSelf=self;
        KisakPrepareCoD4xPatch(root,^(NSError *error){
            KISEngineViewController *strongSelf=weakSelf;if(!strongSelf)return;
            strongSelf->_preparingPatch=NO;
            if(error){strongSelf->_status.text=[NSString stringWithFormat:@"%@\n\nCheck your connection and reopen COD4iOS to retry.",error.localizedDescription];return;}
            strongSelf->_patchReady=YES;[strongSelf startIfReady];
        });
        return;
    }
#endif
    _started = YES;
    CGFloat scale = self.view.window.screen.nativeScale;
#if TARGET_OS_SIMULATOR
    // Optional test setting: reduce render targets on memory-constrained Macs
    // without changing the device build or shrinking menu hit targets.
    if (const char *limit = getenv("KISAK_RENDER_MAX_DIMENSION"))
    {
        const int maximum = atoi(limit);
        const CGFloat longest = MAX(self.view.bounds.size.width, self.view.bounds.size.height);
        if (maximum >= 640 && longest > 0)
            scale = MIN(scale, maximum / longest);
    }
#endif
    const int width = static_cast<int>(self.view.bounds.size.width * scale);
    const int height = static_cast<int>(self.view.bounds.size.height * scale);
    ((CAMetalLayer *)_engineView.layer).drawableSize = CGSizeMake(width, height);
    KisakApple_SetRenderWindow((__bridge void *)_engineView, width, height);
    const UIEdgeInsets insets = self.view.safeAreaInsets;
    // Symmetric margins also protect the HUD after rotating to the other
    // landscape orientation, without changing the world/render viewport.
    KisakApple_SetDisplaySafeArea(MAX(insets.left, insets.right) * scale,
                                 MAX(insets.top, insets.bottom) * scale);

    NSString *logPath = [root stringByAppendingPathComponent:@"kisakcod.log"];
    // Retain the prior session, including failures, when the app is reopened.
    NSString *previousLogPath = [root stringByAppendingPathComponent:@"kisakcod.previous.log"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:logPath]) {
        [[NSFileManager defaultManager] removeItemAtPath:previousLogPath error:nil];
        [[NSFileManager defaultManager] moveItemAtPath:logPath toPath:previousLogPath error:nil];
    }
    freopen(logPath.fileSystemRepresentation, "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    // Share stdout's file description so both streams append in order instead of
    // overwriting each other through separate file offsets.
    dup2(fileno(stdout), fileno(stderr));
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setenv("KISAK_INSTALL_PATH", root.fileSystemRepresentation, 1);
    chdir(root.fileSystemRepresentation);

    _status.text = @"Starting engine… console output: Documents/kisakcod.log";

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    // The decompiled engine keeps large buffers on its main thread's stack.
    pthread_attr_setstacksize(&attributes, 16 * 1024 * 1024);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, KISEngineThreadMain, nullptr) == 0)
    {
        pthread_detach(thread);
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayTick:)];
        // Request the panel's maximum refresh rate (including ProMotion).
        // This link enables high-refresh presentation; it does not gate the engine.
        const float maximumFPS = self.view.window.screen.maximumFramesPerSecond;
        _displayLink.preferredFrameRateRange = CAFrameRateRangeMake(30.0f, maximumFPS, maximumFPS);
        [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
        _controllerInput = [KISControllerInput new];
        _touchControls=[[KISTouchControls alloc] initWithFrame:self.view.bounds];
        [self.view addSubview:_touchControls];
        _controllerInput.touchControls=_touchControls;
        __weak KISControllerInput *input=_controllerInput;
        _touchControls.inputChanged=^{ [input publish]; };
        [_controllerInput start];
        // The engine draws into this view now; the label would cover the menu.
        _status.hidden = YES;
        [_engineView becomeFirstResponder];
        UITapGestureRecognizer *twoFingerTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(toggleKeyboard)];
        twoFingerTap.numberOfTouchesRequired = 2;
        twoFingerTap.delegate = self;
        twoFingerTap.cancelsTouchesInView = NO;
        [_engineView addGestureRecognizer:twoFingerTap];
        [NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(syncKeyboard) userInfo:nil repeats:YES];
    }
    else
        _status.text = @"Could not start the engine thread.";
    pthread_attr_destroy(&attributes);
}

- (void)displayTick:(CADisplayLink *)link
{
    (void)link; // Engine frames are uncapped and paced only by available Metal drawables.
}

- (void)syncKeyboard
{
#if TARGET_OS_SIMULATOR
    static unsigned ticks = 0;
    if (++ticks % 25 == 0)
    {
        task_vm_info_data_t info = {};
        mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
        if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
            fprintf(stderr, "Simulator memory: %llu MiB footprint, %llu MiB resident, %llu MiB compressed\n",
                    static_cast<unsigned long long>(info.phys_footprint / (1024 * 1024)),
                    static_cast<unsigned long long>(info.resident_size / (1024 * 1024)),
                    static_cast<unsigned long long>(info.compressed / (1024 * 1024)));
    }
#endif
    const BOOL editing = KisakApple_TextInputActive() != 0;
    if (editing != _engineEditing)
    {
        _engineEditing = editing;
        fprintf(stderr, "Keyboard: game text field editing %s\n", editing ? "started" : "ended");
        // Follow the game: show the keyboard when a field starts editing, hide it after.
        _engineView.wantsKeyboard = editing;
    }
    if (!_engineView.isFirstResponder)
        [_engineView becomeFirstResponder];
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    (void)gestureRecognizer;
    return KisakApple_ControllerTouchContext()==0;
}

- (void)toggleKeyboard
{
    _engineView.wantsKeyboard = !_engineView.wantsKeyboard;
    fprintf(stderr, "Keyboard: toggled %s by two-finger tap\n", _engineView.wantsKeyboard ? "on" : "off");
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
@end

// iOS 26 and later trap at launch any app that has not adopted the UIScene lifecycle
// (___UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption). The window is therefore
// built when the scene connects rather than in didFinishLaunchingWithOptions; Info.plist.in
// names this class in its UIApplicationSceneManifest.
@interface KISEngineSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation KISEngineSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options
{
    (void)session; (void)options;
    if (![scene isKindOfClass:UIWindowScene.class])
        return;
    UIWindowScene *windowScene = (UIWindowScene *)scene;
    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
    self.window.rootViewController = [KISEngineViewController new];
    [self.window makeKeyAndVisible];
}
@end

@interface KISEngineAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation KISEngineAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options
{
    (void)application; (void)options;
    AVAudioSession *session = AVAudioSession.sharedInstance;
    NSError *error = nil;
    // Game audio uses media volume and stays audible with the Ring/Silent switch on.
    if (![session setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:&error])
        NSLog(@"COD4iOS: audio session category failed: %@", error);
    [self activateGameAudio:nil];
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(activateGameAudio:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(audioInterrupted:) name:AVAudioSessionInterruptionNotification object:session];
    return YES;
}

- (void)activateGameAudio:(NSNotification *)notification
{
    (void)notification;
    NSError *error = nil;
    if (![AVAudioSession.sharedInstance setActive:YES error:&error])
        NSLog(@"COD4iOS: audio session activation failed: %@", error);
}

- (void)audioInterrupted:(NSNotification *)notification
{
    if ([notification.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue] == AVAudioSessionInterruptionTypeEnded &&
        ([notification.userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue] & AVAudioSessionInterruptionOptionShouldResume))
        [self activateGameAudio:nil];
}

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)options
{
    (void)application; (void)options;
    UISceneConfiguration *configuration =
        [[UISceneConfiguration alloc] initWithName:@"Default" sessionRole:session.role];
    configuration.delegateClass = KISEngineSceneDelegate.class;
    return configuration;
}
@end

// The app ships both engines as dylibs and loads one. KISAK_MP is a build-wide ABI switch (it
// changes struct layouts), so singleplayer and multiplayer cannot share a binary; two-level
// namespacing keeps each dylib's symbols and globals to itself. The launcher picks a mode and
// calls this, which is the only symbol either dylib exports.
extern "C" __attribute__((visibility("default"))) int KisakEngine_AppMain(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(KISEngineAppDelegate.class));
    }
}

#ifndef KISAK_ENGINE_AS_DYLIB
int main(int argc, char *argv[])
{
    return KisakEngine_AppMain(argc, argv);
}
#endif

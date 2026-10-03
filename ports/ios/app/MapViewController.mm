#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "MetalRenderer.h"
#include "kisak_core.h"
#include "FastfileWorld.hpp"
#include <memory>
#include <exception>

using namespace kisakcod;

static UIColor *accent() { return [UIColor colorWithRed:0.79 green:0.91 blue:0.42 alpha:1]; }
static UILabel *label(NSString *text,CGFloat size,UIFontWeight weight) {
    UILabel *item=[UILabel new]; item.text=text; item.textColor=UIColor.whiteColor;
    item.font=[UIFont systemFontOfSize:size weight:weight]; item.numberOfLines=0;
    return item;
}
static UIButton *button(NSString *text,id target,SEL action) {
    UIButton *item=[UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *config=[UIButtonConfiguration filledButtonConfiguration];
    config.title=text; config.baseBackgroundColor=[UIColor colorWithWhite:0.16 alpha:0.94];
    config.baseForegroundColor=UIColor.whiteColor; config.cornerStyle=UIButtonConfigurationCornerStyleMedium;
    config.contentInsets=NSDirectionalEdgeInsetsMake(10,16,10,16); item.configuration=config;
    [item addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    return item;
}

@interface KISTouchView : UIView
- (ios::CameraInput)consumeInput;
- (void)cancelInput;
@end
@implementation KISTouchView {
    UITouch *_movementTouch,*_lookTouch;
    CGPoint _origin,_stick,_lookPrevious,_lookDelta;
}
- (instancetype)initWithFrame:(CGRect)frame {
    if((self=[super initWithFrame:frame])) {
        self.multipleTouchEnabled=YES; self.backgroundColor=UIColor.clearColor;
        self.accessibilityLabel=@"Trascina a sinistra per muoverti, a destra per guardare";
    }
    return self;
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches) {
        CGPoint p=[touch locationInView:self];
        if(p.x<self.bounds.size.width*0.5 && !_movementTouch) {
            _movementTouch=touch; _origin=p; _stick=CGPointZero;
        } else if(p.x>=self.bounds.size.width*0.5 && !_lookTouch) {
            _lookTouch=touch; _lookPrevious=p;
        }
    }
    [self setNeedsDisplay];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches) {
        CGPoint p=[touch locationInView:self];
        if(touch==_movementTouch) {
            CGFloat x=p.x-_origin.x,y=p.y-_origin.y;
            CGFloat magnitude=std::max(std::hypot(x,y),55.0);
            _stick=CGPointMake(x/magnitude,y/magnitude);
        } else if(touch==_lookTouch) {
            _lookDelta.x+=p.x-_lookPrevious.x; _lookDelta.y+=p.y-_lookPrevious.y;
            _lookPrevious=p;
        }
    }
    [self setNeedsDisplay];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches) {
        if(touch==_movementTouch) {_movementTouch=nil;_stick=CGPointZero;}
        if(touch==_lookTouch) _lookTouch=nil;
    }
    [self setNeedsDisplay];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
    _lookDelta=CGPointZero;
}
- (void)cancelInput {
    _movementTouch=nil; _lookTouch=nil; _stick=CGPointZero; _lookDelta=CGPointZero;
    [self setNeedsDisplay];
}
- (ios::CameraInput)consumeInput {
    ios::CameraInput input;
    input.strafe=(float)_stick.x; input.forward=(float)-_stick.y;
    input.lookPointsX=(float)_lookDelta.x; input.lookPointsY=(float)_lookDelta.y;
    _lookDelta=CGPointZero;
    return input;
}
- (void)drawRect:(CGRect)rect {
    (void)rect;
    if(!_movementTouch) return;
    CGContextRef context=UIGraphicsGetCurrentContext();
    CGContextSetStrokeColorWithColor(context,[UIColor colorWithWhite:1 alpha:0.3].CGColor);
    CGContextSetLineWidth(context,1.5);
    CGContextStrokeEllipseInRect(context,CGRectMake(_origin.x-55,_origin.y-55,110,110));
    CGContextSetFillColorWithColor(context,[accent() colorWithAlphaComponent:0.65].CGColor);
    CGContextFillEllipseInRect(context,CGRectMake(_origin.x+_stick.x*55-17,_origin.y+_stick.y*55-17,34,34));
}
@end

@interface KISMapViewController : UIViewController <UIDocumentPickerDelegate>
@end
@implementation KISMapViewController {
    MTKView *_metalView;
    KISMetalRenderer *_renderer;
    KISTouchView *_touchView;
    UIStackView *_welcome,*_header,*_footer;
    UILabel *_status,*_mapName;
    UIButton *_open,*_reset,*_up,*_down;
    BOOL _rising,_falling,_loading,_hasMap,_active,_renderFailed;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor=[UIColor colorWithRed:0.035 green:0.055 blue:0.05 alpha:1];
    _active=UIApplication.sharedApplication.applicationState==UIApplicationStateActive;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    _metalView=[[MTKView alloc] initWithFrame:self.view.bounds device:device];
    _metalView.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _metalView.colorPixelFormat=MTLPixelFormatBGRA8Unorm_sRGB;
    _metalView.depthStencilPixelFormat=MTLPixelFormatDepth32Float;
    _metalView.clearColor=MTLClearColorMake(0.012,0.022,0.018,1);
    _metalView.preferredFramesPerSecond=60; _metalView.paused=YES;
    [self.view addSubview:_metalView];
    NSError *error=nil;
    _renderer=[[KISMetalRenderer alloc] initWithView:_metalView error:&error];
    _metalView.delegate=_renderer;
    _touchView=[[KISTouchView alloc] initWithFrame:self.view.bounds];
    _touchView.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _touchView.hidden=YES; [self.view addSubview:_touchView];

    _mapName=label(@"KISAKCOD",18,UIFontWeightBold);
    _open=button(@"Apri mappa",self,@selector(openMap));
    _reset=button(@"Reimposta vista",self,@selector(resetCamera)); _reset.hidden=YES;
    UIView *spacer=[UIView new];
    [_mapName setContentHuggingPriority:UILayoutPriorityDefaultHigh forAxis:UILayoutConstraintAxisHorizontal];
    _header=[[UIStackView alloc] initWithArrangedSubviews:@[_mapName,spacer,_reset,_open]];
    _header.axis=UILayoutConstraintAxisHorizontal; _header.alignment=UIStackViewAlignmentCenter; _header.spacing=12;
    _header.translatesAutoresizingMaskIntoConstraints=NO; [self.view addSubview:_header];

    UILabel *eyebrow=label(@"IPHONE / ANTEPRIMA DEL PORT",11,UIFontWeightBold); eyebrow.textColor=accent();
    UILabel *title=label(@"Il primo passo\nsu iPhone.",34,UIFontWeightBold);
    UILabel *description=label(@"Esplora la geometria delle mappe originali COD4 da un fastfile .ff per PC o da un .d3dbsp.\n\nCampagna e multiplayer non sono ancora implementati.",15,UIFontWeightRegular);
    description.textColor=[UIColor colorWithWhite:0.73 alpha:1];
    UILabel *hint=label(@"Da File scegli per esempio mp_shipment.ff, mantenendo il nome originale. I pacchetti _load.ff e .iwd non contengono una mappa utilizzabile da questa versione.",12,UIFontWeightMedium);
    hint.textColor=[UIColor colorWithWhite:0.55 alpha:1];
    _welcome=[[UIStackView alloc] initWithArrangedSubviews:@[eyebrow,title,description,hint]];
    _welcome.axis=UILayoutConstraintAxisVertical; _welcome.spacing=13;
    _welcome.translatesAutoresizingMaskIntoConstraints=NO; [self.view addSubview:_welcome];

    _status=label(@"Visualizzatore nativo · Metal · Nessuna mappa caricata",11,UIFontWeightMedium);
    _status.textColor=[UIColor colorWithWhite:0.72 alpha:1];
    _up=button(@"Sali",self,@selector(endRise)); _down=button(@"Scendi",self,@selector(endFall));
    [_up addTarget:self action:@selector(beginRise) forControlEvents:UIControlEventTouchDown];
    [_down addTarget:self action:@selector(beginFall) forControlEvents:UIControlEventTouchDown];
    UIControlEvents release=UIControlEventTouchUpOutside|UIControlEventTouchCancel|UIControlEventTouchDragExit;
    [_up addTarget:self action:@selector(endRise) forControlEvents:release];
    [_down addTarget:self action:@selector(endFall) forControlEvents:release];
    _up.hidden=YES; _down.hidden=YES;
    _footer=[[UIStackView alloc] initWithArrangedSubviews:@[_status,_down,_up]];
    _footer.axis=UILayoutConstraintAxisHorizontal; _footer.alignment=UIStackViewAlignmentCenter; _footer.spacing=10;
    _footer.translatesAutoresizingMaskIntoConstraints=NO; [self.view addSubview:_footer];
    UILayoutGuide *safe=self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [_header.topAnchor constraintEqualToAnchor:safe.topAnchor constant:12],
        [_header.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:20],
        [_header.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-20],
        [_welcome.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:30],
        [_welcome.trailingAnchor constraintLessThanOrEqualToAnchor:safe.trailingAnchor constant:-30],
        [_welcome.widthAnchor constraintLessThanOrEqualToConstant:600],
        [_welcome.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor constant:8],
        [_welcome.topAnchor constraintGreaterThanOrEqualToAnchor:_header.bottomAnchor constant:16],
        [_welcome.bottomAnchor constraintLessThanOrEqualToAnchor:_footer.topAnchor constant:-12],
        [_footer.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:20],
        [_footer.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-20],
        [_footer.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-12]
    ]];
    __weak KISMapViewController *weakSelf=self;
    _renderer.readInput=^ios::CameraInput {
        KISMapViewController *controller=weakSelf;
        return controller?[controller readInput]:ios::CameraInput{};
    };
    _renderer.renderFailure=^(NSString *message) {
        KISMapViewController *controller=weakSelf;
        if(!controller || controller->_renderFailed) return;
        controller->_renderFailed=YES;
        [controller updateRunning]; [controller showError:message];
    };
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(didResignActive)
                                              name:UIApplicationWillResignActiveNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(didBecomeActive)
                                              name:UIApplicationDidBecomeActiveNotification object:nil];
    if(!_renderer) {_status.text=error.localizedDescription?:@"Metal non disponibile";_open.enabled=NO;}
    else if(KisakCore_RunSelfTests()!=0) {
        _status.text=@"Verifica del codice MD4/Huffman non riuscita."; _open.enabled=NO;
    }
    if(_open.enabled) {
        NSArray<NSURL *> *bundled=[NSBundle.mainBundle URLsForResourcesWithExtension:@"ff" subdirectory:nil];
        if(!bundled.count) bundled=[NSBundle.mainBundle URLsForResourcesWithExtension:@"d3dbsp" subdirectory:nil];
        if(bundled.count==1) {
            _loading=YES;_open.enabled=NO;_status.text=@"Caricamento della mappa inclusa nella build personale…";
            dispatch_async(dispatch_get_main_queue(),^{[weakSelf loadURL:bundled.firstObject];});
        }
    }
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    // Keep the introductory content readable on compact landscape iPhones.
    UILabel *title=(UILabel *)_welcome.arrangedSubviews[1];
    UILabel *description=(UILabel *)_welcome.arrangedSubviews[2];
    BOOL compact=self.view.bounds.size.height<430;
    title.text=compact?@"Il primo passo su iPhone.":@"Il primo passo\nsu iPhone.";
    title.font=[UIFont systemFontOfSize:compact?26:34 weight:UIFontWeightBold];
    description.font=[UIFont systemFontOfSize:compact?13:15];
    if(compact) {
        description.text=@"Apri una mappa originale COD4 .ff per PC o .d3dbsp per esplorarne la geometria. Campagna, multiplayer, texture e modelli non sono ancora implementati.";
    } else {
        description.text=@"Esplora la geometria delle mappe originali COD4 da un fastfile .ff per PC o da un .d3dbsp.\n\nCampagna, multiplayer, texture e modelli non sono ancora implementati.";
    }
    _welcome.spacing=compact?8:13;
}
- (BOOL)prefersStatusBarHidden {return YES;}
- (BOOL)prefersHomeIndicatorAutoHidden {return _hasMap;}
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {return UIInterfaceOrientationMaskLandscape;}
- (void)dealloc {[NSNotificationCenter.defaultCenter removeObserver:self];}
- (void)beginRise {_rising=YES;}
- (void)endRise {_rising=NO;}
- (void)beginFall {_falling=YES;}
- (void)endFall {_falling=NO;}
- (void)clearInput {[_touchView cancelInput];_rising=NO;_falling=NO;[_renderer resetClock];}
- (void)updateRunning {
    _metalView.paused=!_active || !_hasMap || _loading || _renderFailed || self.presentedViewController!=nil;
    UIApplication.sharedApplication.idleTimerDisabled=!_metalView.paused;
    if(_metalView.paused) [self clearInput];
}
- (void)didResignActive {_active=NO;[self updateRunning];}
- (void)didBecomeActive {_active=YES;[self updateRunning];}
- (void)resetCamera {[self clearInput];[_renderer resetCamera];}
- (ios::CameraInput)readInput {
    ios::CameraInput input=[_touchView consumeInput];
    input.vertical=(_rising?1.0f:0.0f)-(_falling?1.0f:0.0f);
    GCExtendedGamepad *pad=nil;
    for(GCController *controller in GCController.controllers) {
        if(controller.extendedGamepad) {pad=controller.extendedGamepad;break;}
    }
    auto axis=[](float x) {float a=std::abs(x);return a<0.15f?0.0f:std::copysign((a-0.15f)/0.85f,x);};
    if(pad) {
        input.strafe+=axis(pad.leftThumbstick.xAxis.value);
        input.forward+=axis(pad.leftThumbstick.yAxis.value);
        input.turnRateX=axis(pad.rightThumbstick.xAxis.value);
        input.turnRateY=axis(pad.rightThumbstick.yAxis.value);
        input.vertical+=pad.rightTrigger.value-pad.leftTrigger.value;
    }
    return input;
}
- (void)showError:(NSString *)message {
    [self clearInput];
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Impossibile aprire la mappa"
                                        message:message preferredStyle:UIAlertControllerStyleAlert];
    __weak KISMapViewController *weakSelf=self;
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        (void)action;
        [weakSelf dismissViewControllerAnimated:YES completion:^{[weakSelf updateRunning];}];
    }]];
    [self presentViewController:alert animated:YES completion:^{[weakSelf updateRunning];}];
}
- (void)openMap {
    if(_loading) return;
    [self clearInput];
    UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[UTTypeData] asCopy:NO];
    picker.delegate=self; picker.allowsMultipleSelection=NO;
    __weak KISMapViewController *weakSelf=self;
    [self presentViewController:picker animated:YES completion:^{[weakSelf updateRunning];}];
    _metalView.paused=YES;
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    [controller dismissViewControllerAnimated:YES completion:^{[self updateRunning];}];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if(!urls.count) return;
    NSURL *url=urls.firstObject;
    _loading=YES; _open.enabled=NO; [self updateRunning];
    _status.text=@"Lettura e verifica della geometria…";
    [controller dismissViewControllerAnimated:YES completion:^{[self loadURL:url];}];
}
- (void)loadURL:(NSURL *)url {
    // Security-scoped access and coordination let File Provider/iCloud materialize
    // the selected document. Parsing and IO stay off the render/UI thread.
    __weak KISMapViewController *weakSelf=self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        BOOL scoped=[url startAccessingSecurityScopedResource];
        __block std::shared_ptr<assets::BspMesh> mesh;
        __block NSString *failure=nil;
        NSError *coordinationError=nil;
        NSFileCoordinator *coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];
        [coordinator coordinateReadingItemAtURL:url options:0 error:&coordinationError byAccessor:^(NSURL *localURL) {
            try { mesh=std::make_shared<assets::BspMesh>(assets::loadMap(std::string(localURL.fileSystemRepresentation))); }
            catch(const std::exception& error) {failure=[NSString stringWithUTF8String:error.what()];}
            catch(...) {failure=@"Errore inatteso nel caricamento della mappa.";}
        }];
        if(scoped) [url stopAccessingSecurityScopedResource];
        if(coordinationError) failure=coordinationError.localizedDescription;
        if(!mesh && !failure) failure=@"Nessuna geometria disponibile nel file selezionato.";
        dispatch_async(dispatch_get_main_queue(),^{
            KISMapViewController *controller=weakSelf;
            if(!controller) return;
            controller->_loading=NO; controller->_open.enabled=YES;
            NSError *metalError=nil;
            if(mesh && !failure && ![controller->_renderer loadMesh:*mesh error:&metalError]) failure=metalError.localizedDescription;
            if(failure) {
                controller->_status.text=controller->_hasMap?@"Mappa precedente mantenuta":@"File non caricato · Scegli una mappa PC .ff o IBSP 22";
                [controller showError:failure]; return;
            }
            controller->_hasMap=YES; controller->_renderFailed=NO;
            controller->_welcome.hidden=YES; controller->_touchView.hidden=NO;
            controller->_reset.hidden=NO; controller->_up.hidden=NO; controller->_down.hidden=NO;
            controller->_mapName.text=url.lastPathComponent;
            controller->_mapName.lineBreakMode=NSLineBreakByTruncatingMiddle;
            controller->_mapName.numberOfLines=1;
            controller->_status.text=[NSString stringWithFormat:@"%lu triangoli · Sinistra: muovi / destra: guarda · Volo libero, senza collisioni",(unsigned long)(mesh->indices.size()/3)];
            [controller setNeedsUpdateOfHomeIndicatorAutoHidden];
            [controller updateRunning];
        });
    });
}
@end

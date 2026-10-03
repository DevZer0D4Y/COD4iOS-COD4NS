// FPS Compact appearance and positions adapted from the user's MC360-Recomp
// XeniOS/ReXGlue overlay. Copyright 2026 Ben Vanik and MC360-Recomp contributors.
// BSD-3-Clause; see TouchControls.LICENSE.
#import "touch_controls.h"
#import <QuartzCore/QuartzCore.h>
#include "../input/TouchControls.h"

@implementation KISTouchControls {
    kisak::touch::State _input;
    NSMutableArray<UIView *> *_shells;
    UIView *_knob;
    BOOL _available;
    int _context;
}
- (instancetype)initWithFrame:(CGRect)frame {
    if (!(self=[super initWithFrame:frame])) return nil;
    self.multipleTouchEnabled=YES;
    self.backgroundColor=UIColor.clearColor;
    self.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _shells=[NSMutableArray array];
    for (const auto &control:kisak::touch::controls) {
        UIView *shell=[UIView new];
        shell.userInteractionEnabled=NO;
        shell.layer.borderWidth=1.5;
        shell.layer.borderColor=[UIColor colorWithWhite:1 alpha:control.button<0?.38:.32].CGColor;
        shell.backgroundColor=[UIColor colorWithWhite:1 alpha:control.button<0?.10:.08];
        UILabel *label=[UILabel new];
        label.text=[NSString stringWithUTF8String:control.label];
        label.font=[UIFont systemFontOfSize:12 weight:UIFontWeightSemibold];
        label.textColor=[UIColor colorWithWhite:1 alpha:.92];
        label.textAlignment=NSTextAlignmentCenter;
        label.adjustsFontSizeToFitWidth=YES;
        label.minimumScaleFactor=.6;
        label.tag=100;
        [shell addSubview:label];
        if (control.button<0) {
            NSArray *symbols=@[@"chevron.up",@"chevron.down",@"chevron.left",@"chevron.right"];
            for (NSUInteger n=0;n<4;++n) {
                UIImageView *arrow=[[UIImageView alloc] initWithImage:[UIImage systemImageNamed:symbols[n]]];
                arrow.tintColor=UIColor.whiteColor;arrow.alpha=.85;arrow.tag=101+n;
                arrow.contentMode=UIViewContentModeCenter;[shell addSubview:arrow];
            }
        }
        [_shells addObject:shell];[self addSubview:shell];
    }
    _knob=[UIView new];_knob.userInteractionEnabled=NO;
    _knob.backgroundColor=[UIColor colorWithWhite:1 alpha:.18];
    _knob.layer.borderWidth=1.5;
    _knob.layer.borderColor=[UIColor colorWithWhite:1 alpha:.72].CGColor;
    [self addSubview:_knob];self.hidden=YES;
    return self;
}
- (void)layoutSubviews {
    [super layoutSubviews];
    const UIEdgeInsets insets=self.safeAreaInsets;
    CGRect safe=UIEdgeInsetsInsetRect(self.bounds,insets);
    safe=CGRectInset(safe,4,4);
    _input.SetBounds({float(safe.origin.x),float(safe.origin.y),float(safe.size.width),float(safe.size.height)});
    for (size_t i=0;i<kisak::touch::count;++i) {
        const auto frame=_input.Frame(i);UIView *shell=_shells[i];
        shell.frame=CGRectMake(frame.x,frame.y,frame.w,frame.h);
        shell.layer.cornerRadius=frame.w*.5f;
        [shell viewWithTag:100].frame=CGRectInset(shell.bounds,5,5);
        if (i==0) {
            const CGFloat c=frame.w*.5f,r=frame.w*.41f,size=MAX(frame.w*.15f,18);
            const CGPoint centers[]={{c,c-r},{c,c+r},{c-r,c},{c+r,c}};
            for (NSUInteger n=0;n<4;++n)
                [shell viewWithTag:101+n].frame=CGRectMake(centers[n].x-size/2,centers[n].y-size/2,size,size);
        }
    }
    [self refreshVisuals];
    if(self.inputChanged)self.inputChanged();
}
- (void)setAvailable:(BOOL)available context:(int)context {
    if(_available==available && _context==context)return;
    _available=available;_context=context;
    _input.SetContext(available && context!=2,context==1);
    self.hidden=!available || context==2;
    [self refreshVisuals];
}
- (void)refreshVisuals {
    const double now=CACurrentMediaTime();
    for(size_t i=0;i<kisak::touch::count;++i) {
        UIView *shell=_shells[i];const auto &control=kisak::touch::controls[i];
        shell.hidden=_context!=1 && !kisak::touch::MenuControl(i);
        const BOOL pressed=control.button>=0 && _input.Pressed(control.button,now);
        shell.backgroundColor=[UIColor colorWithWhite:1 alpha:pressed?.23:(i==0?.10:.08)];
        shell.layer.borderColor=[UIColor colorWithWhite:1 alpha:pressed?.78:(i==0?.38:.32)].CGColor;
    }
    const auto frame=_input.Frame(0);const auto center=frame.Center();const auto move=_input.MoveKnob();
    const CGFloat side=frame.w*.30f;
    _knob.frame=CGRectMake(center.x+move.x-side*.5f,center.y+move.y-side*.5f,side,side);
    _knob.layer.cornerRadius=side*.5f;
}
- (kisak::controller::Snapshot)sample {
    [self refreshVisuals];
    return _input.Sample(CACurrentMediaTime());
}
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event {
    (void)event;
    return !self.hidden && _input.Hit({float(point.x),float(point.y)})!=-2;
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches) {
        const CGPoint p=[touch locationInView:self];
        _input.Down(reinterpret_cast<uintptr_t>((__bridge void*)touch),{float(p.x),float(p.y)},CACurrentMediaTime());
    }
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches) {
        const CGPoint p=[touch locationInView:self];
        _input.Move(reinterpret_cast<uintptr_t>((__bridge void*)touch),{float(p.x),float(p.y)});
    }
    if(self.inputChanged)self.inputChanged();
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches)_input.Up(reinterpret_cast<uintptr_t>((__bridge void*)touch));
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    for(UITouch *touch in touches)
        _input.CancelTouch(reinterpret_cast<uintptr_t>((__bridge void*)touch));
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)cancelInputs { _input.Cancel();[self refreshVisuals]; }
@end

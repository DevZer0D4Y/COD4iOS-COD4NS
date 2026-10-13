// FPS Compact appearance and positions adapted from the user's MC360-Recomp
// XeniOS/ReXGlue overlay. Copyright 2026 Ben Vanik and MC360-Recomp contributors.
// BSD-3-Clause; see TouchControls.LICENSE.
#import "touch_controls.h"
#import "TouchLayoutStore.h"
#import <QuartzCore/QuartzCore.h>
#include "../input/TouchControls.h"

@implementation KISTouchControls {
    kisak::touch::State _input;
    NSMutableArray<UIView *> *_shells;
    UIView *_knob;
    BOOL _available;
    int _context;
    UIButton *_editButton;
    UIView *_editorBar;
    UISlider *_sizeSlider, *_opacitySlider;
    UILabel *_editorLabel;
    BOOL _editing;
    BOOL _toolsVisible;
    NSInteger _selected;
    CGPoint _dragOffset;
    __weak UITouch *_editorTouch;
    CGRect _safe;
    float _opacity;
    NSArray *_originalLayout;
    float _originalOpacity;

}
- (instancetype)initWithFrame:(CGRect)frame {
    if (!(self=[super initWithFrame:frame])) return nil;
    self.multipleTouchEnabled=YES;
    self.backgroundColor=UIColor.clearColor;
    self.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _selected=-1;_opacity=1;
    NSDictionary *stored=[KISTouchLayoutStore loadAtURL:KISTouchLayoutStore.defaultURL defaults:NSUserDefaults.standardUserDefaults];
    NSArray *saved=stored[@"layout"];
    if(saved.count==kisak::touch::count)for(NSUInteger i=0;i<saved.count;++i){
        NSArray *entry=saved[i];
        if([entry isKindOfClass:NSArray.class] && entry.count==3)
            _input.SetPlacement(i,[entry[0] floatValue],[entry[1] floatValue],[entry[2] floatValue]);
    }
    NSNumber *opacity=stored[@"opacity"];
    if(opacity && std::isfinite(opacity.floatValue))_opacity=std::clamp(opacity.floatValue,.2f,1.f);
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
    [self addSubview:_knob];
    _editButton=[UIButton buttonWithType:UIButtonTypeSystem];
    [_editButton setTitle:@"Edit controls" forState:UIControlStateNormal];
    _editButton.tintColor=UIColor.whiteColor;_editButton.backgroundColor=[UIColor colorWithWhite:0 alpha:.7];
    _editButton.layer.cornerRadius=8;[_editButton addTarget:self action:@selector(toggleEditor) forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_editButton];
    _editorBar=[UIView new];_editorBar.backgroundColor=[UIColor colorWithWhite:0 alpha:.9];_editorBar.hidden=YES;
    _editorLabel=[UILabel new];_editorLabel.textColor=UIColor.whiteColor;_editorLabel.font=[UIFont systemFontOfSize:12];
    _editorLabel.text=@"Drag controls · Size (left) · Opacity (right)";[_editorBar addSubview:_editorLabel];
    _sizeSlider=[UISlider new];_sizeSlider.minimumValue=.5;_sizeSlider.maximumValue=2;_sizeSlider.value=1;
    _sizeSlider.enabled=NO;
    [_sizeSlider addTarget:self action:@selector(changeSize) forControlEvents:UIControlEventValueChanged];[_editorBar addSubview:_sizeSlider];
    _opacitySlider=[UISlider new];_opacitySlider.minimumValue=.2;_opacitySlider.maximumValue=1;_opacitySlider.value=_opacity;
    [_opacitySlider addTarget:self action:@selector(changeOpacity) forControlEvents:UIControlEventValueChanged];[_editorBar addSubview:_opacitySlider];
    for(NSUInteger i=0;i<3;++i){UIButton *b=[UIButton buttonWithType:UIButtonTypeSystem];
        [b setTitle:@[@"Reset",@"Cancel",@"Save"][i] forState:UIControlStateNormal];b.tag=200+i;b.tintColor=UIColor.whiteColor;
        [b addTarget:self action:@selector(editorAction:) forControlEvents:UIControlEventTouchUpInside];[_editorBar addSubview:b];}
    [self addSubview:_editorBar];self.hidden=YES;
    return self;
}
- (void)layoutSubviews {
    [super layoutSubviews];
    const UIEdgeInsets insets=self.safeAreaInsets;
    CGRect safe=UIEdgeInsetsInsetRect(self.bounds,insets);
    safe=CGRectInset(safe,4,4);
    _editButton.frame=_editing?CGRectMake(safe.origin.x,CGRectGetMaxY(safe)-44,130,44):CGRectMake(safe.origin.x,safe.origin.y,130,44);
    _editorBar.frame=CGRectMake(safe.origin.x,safe.origin.y,safe.size.width,90);
    CGFloat w=safe.size.width;_editorLabel.frame=CGRectMake(8,4,MAX(100,w-250),20);
    _sizeSlider.frame=CGRectMake(8,30,MAX(100,(w-270)/2),40);
    _opacitySlider.frame=CGRectMake(16+MAX(100,(w-270)/2),30,MAX(100,(w-270)/2),40);
    for(NSUInteger i=0;i<3;++i)[_editorBar viewWithTag:200+i].frame=CGRectMake(w-240+i*80,24,76,40);
    // Keep editor and gameplay geometry identical; the toolbar overlays it.
    _safe=safe;
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
    _input.SetContext(available && context!=2 && !_editing,context==1);
    self.hidden=!available || context==2;
    if(self.hidden && _editing)[self finishEditing:NO];
    [self refreshVisuals];
}
- (void)refreshVisuals {
    const double now=CACurrentMediaTime();
    for(size_t i=0;i<kisak::touch::count;++i) {
        UIView *shell=_shells[i];const auto &control=kisak::touch::controls[i];
        shell.hidden=!_editing && _context!=1 && !kisak::touch::MenuControl(i);
        shell.alpha=_opacity;
        shell.layer.borderWidth=(_editing && _selected==(NSInteger)i)?3:1.5;
        const BOOL pressed=control.button>=0 && _input.Pressed(control.button,now);
        shell.backgroundColor=[UIColor colorWithWhite:1 alpha:pressed?.23:(i==0?.10:.08)];
        shell.layer.borderColor=[UIColor colorWithWhite:1 alpha:pressed?.78:(i==0?.38:.32)].CGColor;
    }
    _editButton.hidden=!_available || _context==2;_editorBar.hidden=!_editing || !_toolsVisible;
    [_editButton setTitle:_editing?(_toolsVisible?@"Hide tools":@"Show tools"):@"Edit controls" forState:UIControlStateNormal];
    _knob.hidden=_editing;_knob.alpha=_opacity;
    const auto frame=_input.Frame(0);const auto center=frame.Center();const auto move=_input.MoveKnob();
    const CGFloat side=frame.w*.30f;
    _knob.frame=CGRectMake(center.x+move.x-side*.5f,center.y+move.y-side*.5f,side,side);
    _knob.layer.cornerRadius=side*.5f;
}
- (kisak::controller::Snapshot)sample {
    [self refreshVisuals];
    if(_editing)return {};
    return _input.Sample(CACurrentMediaTime());
}
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event {
    (void)event;
    return !self.hidden && (_editing || (!_editButton.hidden && CGRectContainsPoint(_editButton.frame,point)) || _input.Hit({float(point.x),float(point.y)})!=-2);
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    if(_editing){if(_editorTouch)return;UITouch *touch=touches.anyObject;CGPoint p=[touch locationInView:self];
        for(NSInteger i=kisak::touch::count-1;i>=0;--i)if(kisak::touch::Contains(_input.Frame(i),{float(p.x),float(p.y)})){
            _selected=i;_editorTouch=touch;auto c=_input.Frame(i).Center();_dragOffset=CGPointMake(p.x-c.x,p.y-c.y);
            _sizeSlider.enabled=YES;_sizeSlider.value=_input.GetPlacement(i).scale;[self refreshVisuals];break;}return;}
    for(UITouch *touch in touches) {
        const CGPoint p=[touch locationInView:self];
        _input.Down(reinterpret_cast<uintptr_t>((__bridge void*)touch),{float(p.x),float(p.y)},CACurrentMediaTime());
    }
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    if(_editing){if(_selected>=0 && _editorTouch && [touches containsObject:_editorTouch]){CGPoint p=[_editorTouch locationInView:self];auto n=_input.GetPlacement(_selected);
        _input.SetPlacement(_selected,(p.x-_dragOffset.x-_safe.origin.x)/MAX(1,_safe.size.width),
            (p.y-_dragOffset.y-_safe.origin.y)/MAX(1,_safe.size.height),n.scale);[self setNeedsLayout];}return;}
    for(UITouch *touch in touches) {
        const CGPoint p=[touch locationInView:self];
        _input.Move(reinterpret_cast<uintptr_t>((__bridge void*)touch),{float(p.x),float(p.y)});
    }
    if(self.inputChanged)self.inputChanged();
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    if(_editorTouch && [touches containsObject:_editorTouch])_editorTouch=nil;
    for(UITouch *touch in touches)_input.Up(reinterpret_cast<uintptr_t>((__bridge void*)touch));
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    if(_editorTouch && [touches containsObject:_editorTouch])_editorTouch=nil;
    for(UITouch *touch in touches)
        _input.CancelTouch(reinterpret_cast<uintptr_t>((__bridge void*)touch));
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (NSArray *)layoutValues {
    NSMutableArray *out=[NSMutableArray array];for(NSUInteger i=0;i<kisak::touch::count;++i){auto n=_input.GetPlacement(i);[out addObject:@[@(n.x),@(n.y),@(n.scale)]];}return out;
}
- (void)beginEditing {
    if(_editing)return;
    _originalLayout=[self layoutValues];_originalOpacity=_opacity;_editing=YES;_toolsVisible=YES;_selected=-1;
    _sizeSlider.enabled=NO;_opacitySlider.value=_opacity;
    _editorLabel.text=@"Drag to move · Size · Opacity · Save applies to SP + MP";
    _input.SetContext(false,false);[self setNeedsLayout];[self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)toggleEditor {if(!_editing)[self beginEditing];else{_toolsVisible=!_toolsVisible;[self refreshVisuals];}}
- (void)changeSize {if(_selected>=0){auto n=_input.GetPlacement(_selected);_input.SetPlacement(_selected,n.x,n.y,_sizeSlider.value);[self setNeedsLayout];}}
- (void)changeOpacity {_opacity=_opacitySlider.value;[self refreshVisuals];}
- (void)editorAction:(UIButton *)button {
    if(button.tag==200){_input.ResetLayout();_opacity=1;_opacitySlider.value=1;_sizeSlider.value=1;_selected=-1;_sizeSlider.enabled=NO;[self setNeedsLayout];return;}
    [self finishEditing:button.tag==202];
}
- (void)finishEditing:(BOOL)save {
    if(save){NSError *error=nil;
        if(![KISTouchLayoutStore saveLayout:[self layoutValues] opacity:_opacity
            atURL:KISTouchLayoutStore.defaultURL defaults:NSUserDefaults.standardUserDefaults error:&error]){
            _editorLabel.text=@"Could not save controls. Free space and try Save again.";return;
        }}
    else {for(NSUInteger i=0;i<_originalLayout.count;++i){NSArray *n=_originalLayout[i];_input.SetPlacement(i,[n[0] floatValue],[n[1] floatValue],[n[2] floatValue]);}_opacity=_originalOpacity;}
    _editing=NO;_selected=-1;_editorTouch=nil;_input.SetContext(_available && _context!=2,_context==1);[self setNeedsLayout];
    [self refreshVisuals];if(self.inputChanged)self.inputChanged();
}
- (void)cancelInputs { _input.Cancel();[self refreshVisuals]; }
@end

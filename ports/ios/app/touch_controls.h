#pragma once
#import <UIKit/UIKit.h>
#include "../engine/controller_input.h"
@interface KISTouchControls : UIView
@property(nonatomic,copy) void (^inputChanged)(void);
- (void)setAvailable:(BOOL)available context:(int)context;
- (kisak::controller::Snapshot)sample;
- (void)cancelInputs;
@end

#import <MetalKit/MetalKit.h>
#include "../input/Camera.hpp"
#include "BspLoader.hpp"

@interface KISMetalRenderer : NSObject <MTKViewDelegate>
@property(nonatomic, copy) kisakcod::ios::CameraInput (^readInput)(void);
@property(nonatomic, copy) void (^renderFailure)(NSString *message);
- (instancetype)initWithView:(MTKView *)view error:(NSError **)error;
- (BOOL)loadMesh:(const kisakcod::assets::BspMesh &)mesh error:(NSError **)error;
- (void)resetCamera;
- (void)resetClock;
@end

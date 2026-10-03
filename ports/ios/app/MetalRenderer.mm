#import "MetalRenderer.h"
#import <QuartzCore/QuartzCore.h>
#include <cstddef>

using namespace kisakcod;
static NSError *KISError(NSString *message) {
    return [NSError errorWithDomain:@"KisakCOD.Metal" code:1
                           userInfo:@{NSLocalizedDescriptionKey:message}];
}

// First Metal backend milestone: original BSP world positions/normals/colors.
// Materials, texture stages, animated models and the engine's draw commands are
// not implemented here. This shader intentionally does not emulate D3D9 shaders.
static NSString *const shaderSource = @R"metal(
#include <metal_stdlib>
using namespace metal;
struct Vertex { packed_float3 position; packed_float3 normal; packed_float4 color; };
struct Out { float4 position [[position]]; float3 normal; float4 color; };
vertex Out map_vertex(uint id [[vertex_id]], const device Vertex* verts [[buffer(0)]],
                      constant float4x4& matrix [[buffer(1)]]) {
    Out out;
    out.position=matrix*float4(float3(verts[id].position),1);
    out.normal=float3(verts[id].normal);
    out.color=float4(verts[id].color);
    return out;
}
fragment float4 map_fragment(Out in [[stage_in]]) {
    float len=length(in.normal);
    float3 normal=len>0.001f ? in.normal/len : float3(0,0,1);
    float light=0.35f+0.65f*abs(dot(normal,normalize(float3(0.4f,0.6f,1))));
    return float4(max(in.color.rgb,float3(0.22f))*light,1);
}
)metal";

@implementation KISMetalRenderer {
    __weak MTKView *_view;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _queue;
    id<MTLRenderPipelineState> _pipeline;
    id<MTLDepthStencilState> _depth;
    id<MTLBuffer> _vertices, _indices;
    NSUInteger _indexCount;
    ios::Camera _camera, _initialCamera;
    CFTimeInterval _lastFrame;
    dispatch_semaphore_t _inflight;
}
- (instancetype)initWithView:(MTKView *)view error:(NSError **)error {
    if(!(self=[super init])) return nil;
    _view=view;
    _device=view.device;
    _queue=[_device newCommandQueue];
    _inflight=dispatch_semaphore_create(3);
    if(!_device || !_queue) {
        if(error) *error=KISError(@"Metal non è disponibile su questo dispositivo.");
        return nil;
    }
    id<MTLLibrary> library=[_device newLibraryWithSource:shaderSource options:nil error:error];
    if(!library) return nil;
    MTLRenderPipelineDescriptor *descriptor=[MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction=[library newFunctionWithName:@"map_vertex"];
    descriptor.fragmentFunction=[library newFunctionWithName:@"map_fragment"];
    descriptor.colorAttachments[0].pixelFormat=view.colorPixelFormat;
    descriptor.depthAttachmentPixelFormat=view.depthStencilPixelFormat;
    _pipeline=[_device newRenderPipelineStateWithDescriptor:descriptor error:error];
    if(!_pipeline) return nil;
    MTLDepthStencilDescriptor *depth=[MTLDepthStencilDescriptor new];
    depth.depthCompareFunction=MTLCompareFunctionLess;
    depth.depthWriteEnabled=YES;
    _depth=[_device newDepthStencilStateWithDescriptor:depth];
    if(!_depth) {if(error) *error=KISError(@"Impossibile creare il depth buffer Metal.");return nil;}
    return self;
}
- (BOOL)loadMesh:(const assets::BspMesh &)mesh error:(NSError **)error {
    static_assert(sizeof(assets::BspVertex)==40);
    static_assert(offsetof(assets::BspVertex,normal)==12);
    static_assert(offsetof(assets::BspVertex,color)==24);
    if(mesh.vertices.empty() || mesh.indices.empty()) {
        if(error) *error=KISError(@"La mappa non contiene geometria visualizzabile.");return NO;
    }
    id<MTLBuffer> vertices=[_device newBufferWithBytes:mesh.vertices.data()
                                            length:mesh.vertices.size()*sizeof(assets::BspVertex)
                                           options:MTLResourceStorageModeShared];
    id<MTLBuffer> indices=[_device newBufferWithBytes:mesh.indices.data()
                                           length:mesh.indices.size()*sizeof(uint32_t)
                                          options:MTLResourceStorageModeShared];
    if(!vertices || !indices) {
        if(error) *error=KISError(@"Memoria GPU insufficiente per questa mappa.");return NO;
    }
    // Commit the new map only after both allocations succeed.
    _vertices=vertices; _indices=indices; _indexCount=mesh.indices.size();
    _camera.frameBounds(mesh.minBounds,mesh.maxBounds);
    if(mesh.spawn) {
        _camera.position=mesh.spawn->position;
        _camera.position[2]+=60; // Approximate standing view height for inspection.
        // Spawn angles are already COD4 PITCH/YAW/ROLL degrees; only the viewer's
        // own pitch limit is applied, and roll is left out of the free camera.
        _camera.angles={std::clamp(mesh.spawn->angles[PITCH],
                                   -ios::Camera::kPitchLimit,ios::Camera::kPitchLimit),
                        mesh.spawn->angles[YAW],0};
    }
    _initialCamera=_camera;
    [self resetClock];
    return YES;
}
- (void)resetCamera { _camera=_initialCamera; [self resetClock]; }
- (void)resetClock { _lastFrame=0; }
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    (void)view; (void)size;
}
- (void)drawInMTKView:(MTKView *)view {
    // Drop a frame if the GPU is behind; never block the UI thread.
    if(dispatch_semaphore_wait(_inflight,DISPATCH_TIME_NOW)!=0) return;
    id<MTLCommandBuffer> command=[_queue commandBuffer];
    MTLRenderPassDescriptor *pass=view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable=view.currentDrawable;
    if(!command || !pass || !drawable) {dispatch_semaphore_signal(_inflight);return;}
    CFTimeInterval now=CACurrentMediaTime();
    double elapsed=_lastFrame?now-_lastFrame:0;
    _lastFrame=now;
    ios::CameraInput input=self.readInput?self.readInput():ios::CameraInput{};
    _camera.update(input,elapsed);
    id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
    if(!encoder) {dispatch_semaphore_signal(_inflight);return;}
    if(_indexCount) {
        auto matrix=_camera.viewProjection((float)(view.drawableSize.width/std::max(view.drawableSize.height,1.0)));
        [encoder setRenderPipelineState:_pipeline];
        [encoder setDepthStencilState:_depth];
        [encoder setCullMode:MTLCullModeNone];
        [encoder setVertexBuffer:_vertices offset:0 atIndex:0];
        [encoder setVertexBytes:matrix.data() length:sizeof(matrix) atIndex:1];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_indexCount
                            indexType:MTLIndexTypeUInt32 indexBuffer:_indices indexBufferOffset:0];
    }
    [encoder endEncoding];
    [command presentDrawable:drawable];
    dispatch_semaphore_t semaphore=_inflight;
    __weak KISMetalRenderer *weakSelf=self;
    [command addCompletedHandler:^(id<MTLCommandBuffer> done) {
        dispatch_semaphore_signal(semaphore);
        if(done.status==MTLCommandBufferStatusError) {
            NSString *message=done.error.localizedDescription?:@"Errore durante il rendering Metal.";
            dispatch_async(dispatch_get_main_queue(),^{
                KISMetalRenderer *renderer=weakSelf;
                if(renderer.renderFailure) renderer.renderFailure(message);
            });
        }
    }];
    [command commit];
}
@end

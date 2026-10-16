#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "../../d3d9/metal/dx9_msl_translator.h"
#include "../../d3d9/metal/ShaderCompileOptions.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

// COD4's fog shader finishes with MAD color, fogWeight, lighting, fogColor.
// Unused lighting lanes may contain NaN/Inf; a zero fog weight must mask them.
static kisak::metal::TranslatedShader PixelShader(bool mad)
{
    std::vector<uint32_t> words{0xffff0300, mad ? 0x04000004u : 0x03000005u,
        0x800f0800, 0xa0e40000, 0xa0e40001};
    if(mad)words.push_back(0xa0e40002);
    words.push_back(0xffff);
    kisak::metal::TranslatedShader shader;
    assert(kisak::metal::TranslateShader(words.data(),words.size(),{},shader));
    return shader;
}

int main()
{
    @autoreleasepool {
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();
        if(!device){puts("SKIP: no Metal device");return 77;}
        auto queue=[device newCommandQueue];
        auto options=kisak::metal::ShaderCompileOptions();
        NSError *error=nil;
        auto library=[device newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal; struct V {float4 p [[position]];};vertex V vertexMain(uint n [[vertex_id]]) { float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return V{float4(p[n],0,1)};}" options:options error:&error];
        if(!library){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
        auto vertex=[library newFunctionWithName:@"vertexMain"];
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;
        auto target=[device newTextureWithDescriptor:td];assert(target);
        unsigned failures=0,cases=0;
        for(bool mad:{false,true}) {
            const auto shader=PixelShader(mad);
            auto ps=[device newLibraryWithSource:[NSString stringWithUTF8String:shader.source.c_str()] options:options error:&error];
            if(!ps){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
            auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=vertex;
            pd.fragmentFunction=[ps newFunctionWithName:@"kisak_main"];
            pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA32Float;
            auto pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];assert(pipeline);
            // Include signed zero, both operand orders and nonzero finite controls.
            for(float factor:{0.f,-0.f,.25f,1.f})for(float value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),.5f,-.5f})for(bool swap:{false,true}) {
                float C[224][4]{};const float fog[4]={.8f,.7f,.6f,1.f};
                for(int i=0;i<4;++i){C[0][i]=swap?value:factor;C[1][i]=swap?factor:value;C[2][i]=fog[i];}
                auto cb=[device newBufferWithBytes:C length:sizeof(C) options:MTLResourceStorageModeShared];
                auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=target;
                pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
                auto command=[queue commandBuffer];auto encoder=[command renderCommandEncoderWithDescriptor:pass];
                [encoder setRenderPipelineState:pipeline];[encoder setFragmentBuffer:cb offset:0 atIndex:0];
                kisak::metal::FragmentParams params{};[encoder setFragmentBytes:&params length:sizeof(params) atIndex:1];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
                [command commit];[command waitUntilCompleted];assert(command.status==MTLCommandBufferStatusCompleted);
                float actual[4];[target getBytes:actual bytesPerRow:sizeof(actual) fromRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0];
                for(int i=0;i<4;++i){const float expected=(factor==0?0:factor*value)+(mad?fog[i]:0);
                    const bool match=std::isnan(expected)?std::isnan(actual[i])
                        :std::isinf(expected)?(actual[i]==expected)
                        :std::isfinite(actual[i]) && fabs(actual[i]-expected)<=1e-5f;
                    if(!match)++failures;}
                ++cases;
            }
        }
        printf("Fog/output zero mask: %u GPU cases, %u bad channels, %s\n",cases,failures,device.name.UTF8String);
        return failures?1:0;
    }
}

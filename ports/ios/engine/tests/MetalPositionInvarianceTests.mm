#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <vector>
#include <cstdio>
#include <cassert>
struct V {float p[4];};
#include "../../d3d9/metal/dx9_msl_translator.h"
#include "../../d3d9/metal/ShaderCompileOptions.h"
#include <initializer_list>

// Small authored SM3 programs model COD4's shared world/projection transforms.
// The lighting variant also needs the intermediate position to compute fog.
static kisak::metal::TranslatedShader VertexShader(bool fog)
{
    auto reg=[](unsigned type,unsigned index,unsigned bits) {
        return 0x80000000u | ((type&7)<<28) | ((type&24)<<8) | index | (bits<<16);
    };
    auto dst=[&](unsigned type,unsigned index,unsigned mask=15){return reg(type,index,mask);};
    auto src=[&](unsigned type,unsigned index,unsigned swizzle=0xe4){return reg(type,index,swizzle);};
    std::vector<uint32_t> words{0xfffe0300};
    auto emit=[&](unsigned opcode,std::initializer_list<uint32_t> operands){
        words.push_back(opcode | (uint32_t(operands.size())<<24));
        words.insert(words.end(),operands.begin(),operands.end());
    };
    emit(31,{0x80000000,dst(1,0)}); // dcl_position v0
    emit(31,{0x80000000,dst(6,0)}); // dcl_position o0
    if(fog)emit(31,{0x80000005,dst(6,1)}); // dcl_texcoord o1
    for(unsigned i=0;i<4;++i)emit(9,{dst(0,0,1u<<i),src(1,0),src(2,4+i)});
    for(unsigned i=0;i<4;++i)emit(9,{dst(6,0,1u<<i),src(0,0),src(2,i)});
    if(fog){
        emit(8,{dst(0,1,1),src(0,0),src(0,0)});
        emit(7,{dst(0,1,1),src(0,1,0)});
        emit(6,{dst(0,1,1),src(0,1,0)});
        emit(4,{dst(0,1,1),src(0,1,0),src(2,21,0xaa),src(2,21,0xff)});
        emit(14,{dst(6,1,8)|(1u<<20),src(0,1,0)});
        emit(1,{dst(6,1,7),src(0,0)});
    }
    words.push_back(0xffff);
    kisak::metal::TranslatedShader translated;
    assert(kisak::metal::TranslateShader(words.data(),words.size(),{},translated));
    assert(translated.source.find("[[position, invariant]]")!=std::string::npos);
    return translated;
}

int main(){@autoreleasepool {
 id<MTLDevice>d=MTLCreateSystemDefaultDevice();
 if(!d){puts("SKIP: no Metal device");return 77;}
 auto q=[d newCommandQueue];
 auto opts=kisak::metal::ShaderCompileOptions();assert(opts.preserveInvariance);
 auto compile=[&](bool fog){
    const auto shader=VertexShader(fog);NSError *error=nil;
    id<MTLLibrary>library=[d newLibraryWithSource:[NSString stringWithUTF8String:shader.source.c_str()] options:opts error:&error];
    if(!library){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);abort();}
    return [library newFunctionWithName:@"kisak_main"];
 };
 auto vs0=compile(false),vs1=compile(true);
 NSError *error=nil;auto lib=[d newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal; struct I {float4 fog [[user(texcoord0)]];};fragment float4 color(I in [[stage_in]]){return float4(in.fog.w,1,1,1);}" options:opts error:&error];assert(lib);auto fs=[lib newFunctionWithName:@"color"];
 auto desc=[MTLRenderPipelineDescriptor new];auto vdesc=[MTLVertexDescriptor vertexDescriptor];vdesc.attributes[0].format=MTLVertexFormatFloat4;vdesc.attributes[0].bufferIndex=0;vdesc.layouts[0].stride=sizeof(V);desc.vertexDescriptor=vdesc;desc.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;desc.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
 desc.vertexFunction=vs0;desc.fragmentFunction=nil;desc.colorAttachments[0].writeMask=MTLColorWriteMaskNone;auto pre=[d newRenderPipelineStateWithDescriptor:desc error:&error];assert(pre);
 desc.vertexFunction=vs1;desc.fragmentFunction=fs;desc.colorAttachments[0].writeMask=MTLColorWriteMaskAll;auto lit=[d newRenderPipelineStateWithDescriptor:desc error:&error];assert(lit);
 auto makeDepth=[&](MTLCompareFunction f,bool write){auto a=[MTLDepthStencilDescriptor new];a.depthCompareFunction=f;a.depthWriteEnabled=write;return [d newDepthStencilStateWithDescriptor:a];};auto depthWrite=makeDepth(MTLCompareFunctionLessEqual,true),depthEqual=makeDepth(MTLCompareFunctionEqual,false),depthAlways=makeDepth(MTLCompareFunctionAlways,false);
 const unsigned W=128,H=128;auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:W height:H mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;auto color=[d newTextureWithDescriptor:td];td.pixelFormat=MTLPixelFormatDepth32Float;td.storageMode=MTLStorageModePrivate;auto z=[d newTextureWithDescriptor:td];
 unsigned totalMissing=0,totalReference=0;for(float eye:{0.f,8192.25f,65536.125f,1000000.25f})for(float distance:{8.f,128.f,2048.f,8192.f,32768.f}){
  std::vector<V>vertices;for(int y=0;y<8;++y)for(int x=0;x<8;++x){float xx=-.9f+x*.225f,yy=-.9f+y*.225f;for(int k:{0,1,2,2,1,3}){V a{};a.p[0]=eye+(xx+(k&1)*.21f)*distance;a.p[1]=-eye+(yy+(k>>1)*.21f)*distance;a.p[2]=eye+distance;a.p[3]=1;vertices.push_back(a);}}
  float C[256][4]{};C[0][0]=1;C[1][1]=1;C[2][2]=1.0000001f;C[2][3]=-4.0000004f;C[3][2]=1;C[4][0]=C[5][1]=C[6][2]=C[7][3]=1;C[4][3]=-eye;C[5][3]=eye;C[6][3]=-eye;C[21][1]=1;C[21][2]=-.001f;
  auto vb=[d newBufferWithBytes:vertices.data() length:vertices.size()*sizeof(V) options:MTLResourceStorageModeShared];auto cb=[d newBufferWithBytes:C length:sizeof(C) options:MTLResourceStorageModeShared];float half[2]={-1.f/W,1.f/H};std::vector<unsigned char> pixels[2];
  for(int reference=0;reference<2;++reference){auto p=[MTLRenderPassDescriptor renderPassDescriptor];p.colorAttachments[0].texture=color;p.colorAttachments[0].loadAction=MTLLoadActionClear;p.colorAttachments[0].storeAction=MTLStoreActionStore;p.depthAttachment.texture=z;p.depthAttachment.loadAction=MTLLoadActionClear;p.depthAttachment.storeAction=MTLStoreActionDontCare;p.depthAttachment.clearDepth=1;
   auto cmd=[q commandBuffer];auto enc=[cmd renderCommandEncoderWithDescriptor:p];[enc setViewport:MTLViewport{0,0,W,H,0,1}];[enc setCullMode:MTLCullModeNone];[enc setVertexBuffer:vb offset:0 atIndex:0];[enc setVertexBuffer:cb offset:0 atIndex:30];[enc setVertexBytes:half length:sizeof(half) atIndex:29];
   if(!reference){[enc setRenderPipelineState:pre];[enc setDepthStencilState:depthWrite];[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];}
   [enc setRenderPipelineState:lit];[enc setDepthStencilState:reference?depthAlways:depthEqual];[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];[enc endEncoding];[cmd commit];[cmd waitUntilCompleted];assert(cmd.status==MTLCommandBufferStatusCompleted);pixels[reference].resize(W*H*4);[color getBytes:pixels[reference].data() bytesPerRow:W*4 fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];}
  unsigned missing=0,ref=0;for(unsigned i=0;i<W*H;++i)if(pixels[1][i*4+1]){++ref;if(!pixels[0][i*4+1])++missing;}totalMissing+=missing;totalReference+=ref;printf("eye %.3f distance %.0f: %u/%u missing pixels\n",eye,distance,missing,ref);
 }
 printf("Invariant depth/lighting positions: missing=%u reference=%u device=%s\n",totalMissing,totalReference,d.name.UTF8String);assert(totalReference>200000);return totalMissing?1:0;
}}

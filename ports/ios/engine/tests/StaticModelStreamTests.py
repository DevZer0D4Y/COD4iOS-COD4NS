#!/usr/bin/env python3
"""Exercise the production decoder, including the exact build 9 fault pointers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[4]
source = (root/'src/gfx_d3d/r_draw_staticmodel.cpp').read_text()
def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n',start)+3]

code = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#define __cdecl
#define iassert(x) assert(x)
#define CON_CHANNEL_GFX 1
struct XSurface {uint16_t tileMode,vertCount,triCount;};
struct XModel {XSurface *surfs;uint8_t numsurfs;};
struct GfxStaticModelDrawInst {XModel *model;uint8_t reflectionProbeIndex;};
struct World {struct {GfxStaticModelDrawInst *smodelDrawInsts;uint32_t smodelCount;}dpvs;};
struct {World *world;}rgp;
struct GfxBackEndData {uint32_t primDrawSurfsBuf[65536];};
struct GfxStaticModelDrawStream {const uint32_t *primDrawSurfPos,*primDrawSurfEnd;XSurface *localSurf;uint32_t smodelCount;const uint16_t *smodelList;uint32_t reflectionProbeIndex;};
struct {uint32_t geoIndexCount=0;}g_frameStatsCur;
struct Stats {uint32_t dynamicIndexCount=0,dynamicVertexCount=0;}stats,*g_primStats=&stats;
unsigned warnings=0;void Com_PrintWarning(int,const char*,...){++warnings;}
namespace kisak::savefile::diagnostics {void Stage(const char*,const char*){}}
struct Cached {uint16_t smodelIndex;};Cached cachedEntries[16384];
const Cached *R_GetCachedSModelSurf(uint32_t index){assert(index && index<=16384);return cachedEntries+index-1;}
'''
for signature in ['static void R_InitStaticModelDrawStream(', 'static bool R_RejectStaticModelRecord(', 'static bool R_ReadStaticModelRecord(', 'int __cdecl R_GetNextStaticModelSurf(', 'int __cdecl R_GetNextStaticModelCachedSurf(']:
    code += function(signature)
code += r'''
int main(){
 GfxBackEndData data{};XSurface surfaces[2]{{0,8,4},{0,12,6}};XModel model{surfaces,2};GfxStaticModelDrawInst instances[2]{{&model,3},{&model,5}};World world{{instances,2}};rgp.world=&world;
 GfxStaticModelDrawStream stream{};XSurface *out=nullptr;auto *p=data.primDrawSurfsBuf;
 auto write=[&](uintptr_t address,uint32_t count){memset(p,0,512*sizeof(uint32_t));p[0]=count;p[1]=uint32_t(address);p[2]=uint32_t(uint64_t(address)>>32);R_InitStaticModelDrawStream(&stream,p,&data);};
 write(uintptr_t(surfaces+1),1);assert(R_GetNextStaticModelSurf(&stream,&out)==1 && out==surfaces+1);assert(!R_GetNextStaticModelSurf(&stream,&out));assert(stats.dynamicIndexCount==18 && stats.dynamicVertexCount==12);
 // Native-crash.txt reports these fault addresses; the dereference is +4.
 for(uintptr_t fault:{0x0000000100001cffULL,0x00002e7b00000005ULL,0x00004deb00000005ULL}){write(fault-4,1);assert(!R_GetNextStaticModelSurf(&stream,&out));}
 write(uintptr_t(surfaces),128);assert(R_GetNextStaticModelSurf(&stream,&out));assert(stream.smodelCount==128 && stream.primDrawSurfPos==p+67);
 write(uintptr_t(surfaces),0xffffffffU);assert(!R_GetNextStaticModelSurf(&stream,&out));
 write(uintptr_t(surfaces),1);p[3]=65535;assert(!R_GetNextStaticModelSurf(&stream,&out));
 write(uintptr_t(surfaces)+1,1);assert(!R_GetNextStaticModelSurf(&stream,&out));
 write(uintptr_t(surfaces),1);instances[0].model=nullptr;assert(!R_GetNextStaticModelSurf(&stream,&out));instances[0].model=&model;
 write(uintptr_t(surfaces),1);p[510]=1;R_InitStaticModelDrawStream(&stream,p+510,&data);assert(!R_GetNextStaticModelSurf(&stream,&out));
 R_InitStaticModelDrawStream(&stream,p+65536,&data);assert(!R_GetNextStaticModelSurf(&stream,&out));
 R_InitStaticModelDrawStream(&stream,reinterpret_cast<uint32_t*>(uintptr_t(p)+1),&data);assert(!R_GetNextStaticModelSurf(&stream,&out));
 write(uintptr_t(surfaces),1);p[511]=0;R_InitStaticModelDrawStream(&stream,p+511,&data);assert(!R_GetNextStaticModelSurf(&stream,&out));
 write(uintptr_t(surfaces),1);p[3]=1;assert(R_GetNextStaticModelCachedSurf(&stream)==1 && stream.reflectionProbeIndex==3);
 write(uintptr_t(surfaces),1);p[3]=16385;assert(!R_GetNextStaticModelCachedSurf(&stream));
 write(uintptr_t(surfaces),1);p[3]=0;assert(!R_GetNextStaticModelCachedSurf(&stream));
 write(uintptr_t(surfaces),1);p[3]=1;cachedEntries[0].smodelIndex=65535;assert(!R_GetNextStaticModelCachedSurf(&stream));
 puts("Production static-model streams: valid geometry preserved; recorded fault pointers, bad IDs/counts and truncated blocks rejected before dereference.");
}
'''
# The test body uses initializer_list, independently of production includes.
code = '#include <initializer_list>\n' + code

def run():
    with tempfile.TemporaryDirectory(prefix='cod4-model-stream-') as directory:
        p=Path(directory);(p/'test.cpp').write_text(code)
        subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)

if __name__ == '__main__':
    run()

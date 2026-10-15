#!/usr/bin/env python3
"""Run production dynamic-light collectors/command writers on a 64-bit host."""
from pathlib import Path
import subprocess
import tempfile
import sys

root = Path(__file__).resolve().parents[4]
def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    end = source.index('\n}\n', start) + 3
    return source[start:end]

code = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#define __cdecl
#define iassert(x) assert(x)
#define bcassert(x,n) assert((x) < (n))
#define ARRAY_COUNT(x) (sizeof(x)/sizeof((x)[0]))
#define HIDWORD(x) (((uint32_t*)&(x))[1])
#define MTL_SORT_OBJECT_ID_BITS 16
#define MAX_VISIBLE_SHADOWABLE_DLIGHTS 1
#define R_WARN_PRIM_DRAW_SURF_BUFFER_SIZE 1
#define R_WARN_MAX_SCENE_DRAWSURFS 2
enum { SF_TRIANGLES=0, SF_STATICMODEL_RIGID=3, GFX_LIGHT_TYPE_OMNI=2, GFX_LIGHT_TYPE_SPOT=3, TECHNIQUE_LIGHT_OMNI=4 };
struct GfxDrawSurfFields {uint64_t objectId:16,reflectionProbeIndex:8,customIndex:5,materialSortedIndex:11,prepass:2,primaryLightIndex:8,surfType:4,primarySortKey:6,unused:4;};
union GfxDrawSurf {GfxDrawSurfFields fields;uint64_t packed;};
struct GfxDrawSurfList {GfxDrawSurf *current,*end;};
struct GfxDelayedCmdBuf {int primDrawSurfPos;uint32_t primDrawSurfSize;GfxDrawSurf drawSurfKey;};
struct GfxBspDrawSurfData {GfxDelayedCmdBuf delayedCmdBuf;GfxDrawSurfList drawSurfList;};
struct GfxVisibleLight {int drawSurfCount;GfxDrawSurf drawSurfs[1024];};
struct Scene {GfxVisibleLight visLight[4],visLightShadow[1];uint64_t guard[2048];} scene;
struct Backend {uint32_t primDrawSurfsBuf[65536];int primDrawSurfPos;} backend,*frontEndDataOut=&backend;
struct XSurface {uint16_t tileMode,vertCount,triCount;};
struct XModel {};
struct GfxStaticModelDrawInst {XModel *model;};
struct GfxStaticModelId {unsigned objectId,surfType;};
struct Material {struct {GfxDrawSurf drawSurf;}info;};
struct GfxSurface {int id;};
struct World {unsigned surfaceCount;struct {uint8_t *surfaceVisData[1],*smodelVisData[1];GfxDrawSurf *surfaceMaterials;GfxSurface *surfaces;GfxStaticModelDrawInst *smodelDrawInsts;uint32_t *lodData;}dpvs;};
struct {World *world;Material *sortedMaterials[2048];} rgp;
struct GfxLight {int type;float origin[3],radius;};
struct {uint8_t *smodelVisData;float position[3],radiusSq,planes[6][4];} g_staticModelLightCallback;
struct BspOmniLightCallback {uint8_t *surfaceVisData;float position[3],radiusSq;};
struct BspSpotLightCallback {uint8_t *surfaceVisData;float planes[6][4];};
struct Dvar {struct {bool enabled;}current;} yes{{true}},no{{false}};
auto *r_spotLightShadows=&yes,*r_spotLightSModelShadows=&yes,*r_pretess=&no;
struct {bool deviceLost=false;} dx;
void MyAssertHandler(const char*,int,int,const char*,...){abort();}
int InterlockedExchangeAdd(int *p,int n){int old=*p;*p+=n;return old;}
void R_WarnOncePerFrame(int,...){ }
int allowSurf_0[1],allowSurf_1[2];
void R_CalcSpotLightPlanes(const GfxLight*,float (*)[4]){}
bool R_SortBspShadowReceiverSurfaces(const GfxSurface *a,const GfxSurface *b){return a->id < b->id;}
int bspCount=1024,modelCount=128,draws=0;
void R_BoxSurfaces(const float*,const float*,const int*,void*,GfxSurface ***out,unsigned capacity,uint32_t *counts,unsigned sets){
 assert(capacity==1024);for(unsigned j=0;j<sets;++j){counts[j]=bspCount;for(int i=0;i<bspCount;++i)out[j][i]=&rgp.world->dpvs.surfaces[i];}
 // Scratch collection must never overwrite adjacent light draw lists or scene data.
 for(auto word:scene.guard)assert(word==0x1234567812345678ULL);
}
bool R_PreTessBspDrawSurfs(GfxDrawSurf,const uint16_t*,uint32_t,GfxBspDrawSurfData*){return false;}
int R_AllowStaticModelOmniLight(int){return 1;}
int R_AllowStaticModelSpotLight(int){return 1;}
int R_BoxStaticModels(const float*,const float*,int(*)(int),uint16_t *out,int capacity){assert(modelCount<=capacity);for(int i=0;i<modelCount;++i)out[i]=i;return modelCount;}
XSurface surface{0,12,4};
int XModelGetSurfaces(XModel*,XSurface **out,int){*out=&surface;return 1;}
GfxStaticModelId R_GetStaticModelId(int index,int){return {unsigned(index),SF_STATICMODEL_RIGID};}
Material modelMaterial;Material *modelMaterials[1]={&modelMaterial};
Material *const* XModelGetSkins(XModel*,int){return modelMaterials;}
bool Material_GetTechnique(const Material*,int){return true;}
void R_InitDelayedCmdBuf(GfxDelayedCmdBuf*);
void R_EndCmdBuf(GfxDelayedCmdBuf*);
int R_AllocDrawSurf(GfxDelayedCmdBuf*,GfxDrawSurf,GfxDrawSurfList*,uint32_t);
void R_WritePrimDrawSurfInt(GfxDelayedCmdBuf*,uint32_t);
void R_WritePrimDrawSurfData(GfxDelayedCmdBuf*,uint8_t*,uint32_t);
'''
code += function('src/gfx_d3d/r_add_staticmodel.h', 'constexpr uint32_t R_StaticModelDrawSurfWords(')
for name in ['R_InitDelayedCmdBuf', 'R_EndCmdBuf', 'R_AllocDrawSurf', 'R_WritePrimDrawSurfInt', 'R_WritePrimDrawSurfData']:
    ret = 'int' if name == 'R_AllocDrawSurf' else 'void'
    code += function('src/gfx_d3d/r_add_cmdbuf.cpp', f'{ret} __cdecl {name}(')
code += function('src/gfx_d3d/r_add_bsp.cpp', 'void __cdecl R_InitBspDrawSurf(')
code += function('src/gfx_d3d/r_add_bsp.cpp', 'void __cdecl R_AddBspDrawSurfs(')
code += function('src/gfx_d3d/r_add_staticmodel.cpp', 'void __cdecl R_AddDelayedStaticModelDrawSurf(')
for name in ['R_GetBspOmniLightSurfs', 'R_GetBspSpotLightSurfs', 'R_GetStaticModelLightSurfs']:
    code += function('src/gfx_d3d/r_light.cpp', f'void __cdecl {name}(')
code += r'''
int main(int argc,char **argv){assert(argc==2);static GfxSurface surfaces[1024];static GfxDrawSurf materials[1024];static GfxStaticModelDrawInst models[1024];static uint32_t lods[64];static uint8_t vis[1024];
 World world{};world.surfaceCount=1024;world.dpvs.surfaces=surfaces;world.dpvs.surfaceMaterials=materials;world.dpvs.smodelDrawInsts=models;world.dpvs.lodData=lods;world.dpvs.surfaceVisData[0]=vis;world.dpvs.smodelVisData[0]=vis;rgp.world=&world;
 for(int i=0;i<1024;++i){surfaces[i].id=i;materials[i].fields.materialSortedIndex=i;}
 modelMaterial.info.drawSurf.fields.materialSortedIndex=1024;rgp.sortedMaterials[1024]=&modelMaterial;
 std::fill(std::begin(scene.guard),std::end(scene.guard),0x1234567812345678ULL);
 GfxBspDrawSurfData data[2]{};R_InitBspDrawSurf(data);R_InitBspDrawSurf(data+1);
 GfxLight lights[4]{};const GfxLight *ptrs[4];for(int i=0;i<4;++i){lights[i].type=GFX_LIGHT_TYPE_OMNI;ptrs[i]=lights+i;}
 if(!strcmp(argv[1],"bsp-omni")){
   R_GetBspOmniLightSurfs(lights,3,data);assert(scene.visLight[3].drawSurfCount==1024);
   for(auto &l:scene.visLightShadow)assert(l.drawSurfCount==0);
   for(int i=0;i<1024;++i)assert(scene.visLight[3].drawSurfs[i].fields.materialSortedIndex==unsigned(i));
 }else if(!strcmp(argv[1],"bsp-spot")){
   lights[0].type=GFX_LIGHT_TYPE_SPOT;R_GetBspSpotLightSurfs(lights,0,data);assert(scene.visLight[0].drawSurfCount==1024 && scene.visLightShadow[0].drawSurfCount==1024);
   for(int i=0;i<1024;++i)assert(scene.visLight[0].drawSurfs[i].fields.materialSortedIndex==unsigned(i) && scene.visLightShadow[0].drawSurfs[i].fields.materialSortedIndex==unsigned(i));
 }else if(!strcmp(argv[1],"model-omni")){
   R_GetStaticModelLightSurfs(ptrs,4);assert(scene.visLightShadow[0].drawSurfCount==0);
   for(auto &l:scene.visLight)assert(l.drawSurfCount==2);
 }else if(!strcmp(argv[1],"full-list")){
   scene.visLight[0].drawSurfCount=1024;scene.visLight[1].drawSurfCount=77;
   R_GetStaticModelLightSurfs(ptrs,1);assert(scene.visLight[0].drawSurfCount==1024 && scene.visLight[1].drawSurfCount==77);
 }else if(!strcmp(argv[1],"model-spot")){
   lights[0].type=GFX_LIGHT_TYPE_SPOT;R_GetStaticModelLightSurfs(ptrs,1);assert(scene.visLightShadow[0].drawSurfCount>0);
   // 128 one-instance records need four words each PLUS a zero terminator;
   // the last record must start a new 512-word allocation instead of spilling.
   const auto &l=scene.visLightShadow[0];unsigned seen=0;
   for(int i=0;i<l.drawSurfCount;++i){auto *p=backend.primDrawSurfsBuf+l.drawSurfs[i].fields.objectId;auto *end=backend.primDrawSurfsBuf+((l.drawSurfs[i].fields.objectId/512)+1)*512;
     while(p<end && *p){assert(*p==1);auto value=uintptr_t(p[1]) | (uint64_t(p[2])<<32);assert(value==uintptr_t(&surface));++seen;p+=4;}
     assert(p<end && *p==0);
   }assert(seen==128);
 }else abort();
 for(auto word:scene.guard)assert(word==0x1234567812345678ULL);
 puts(argv[1]);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-dynamic-lights-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++17','-Wno-sign-compare','-Wno-tautological-constant-out-of-range-compare','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    for case in sys.argv[1:] or ['bsp-omni','bsp-spot','model-omni','model-spot','full-list']:
        subprocess.run([str(p/'test'),case],check=True)
print('Production dynamic lighting: 1,024 BSP receivers, four omni lights, and 128 spotlight model records stay within storage.')

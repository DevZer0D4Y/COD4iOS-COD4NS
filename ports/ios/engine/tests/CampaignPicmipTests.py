from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[4]
s=(repo/'src/gfx_d3d/r_image.cpp').read_text()
production=s[s.index('void __cdecl R_SetPicmip()'):s.index('void R_InitRawImage()')]
stubs=r'''
#include <cassert>
#include <algorithm>
#define __cdecl
#define iassert(x) assert(x)
constexpr int CON_CHANNEL_GFX=0;
struct Dvar {struct {bool enabled=false;int integer=0;} current;};
Dvar reflection,manual,color,bump,spec,specular,renderer;
const Dvar *r_reflectionProbeGenerate=&reflection,*r_picmip_manual=&manual,*r_picmip=&color,*r_picmip_bump=&bump,*r_picmip_spec=&spec,*r_specular=&specular,*r_rendererInUse=&renderer;
struct {void *device=(void*)1;} dx;
struct {int picmip,picmipBump,picmipSpec;} imageGlobals;
using uint32_t=unsigned;
unsigned R_AvailableTextureMemory(){return 1024;}
int Dvar_GetInt(const char*){return 1024;}
void Dvar_SetInt(const Dvar *d,int v){const_cast<Dvar*>(d)->current.integer=v;}
void Com_Printf(int,const char*,...){}
'''
tests=r'''
int main(){
 specular.current.enabled=true;renderer.current.integer=1;
 R_SetPicmip();assert(imageGlobals.picmip==EXPECT_AUTO && imageGlobals.picmipBump==EXPECT_AUTO && imageGlobals.picmipSpec==EXPECT_AUTO);
 assert(color.current.integer==EXPECT_AUTO && bump.current.integer==EXPECT_AUTO && spec.current.integer==EXPECT_AUTO);
 manual.current.enabled=true;color.current.integer=2;bump.current.integer=1;spec.current.integer=3;
 R_SetPicmip();assert(imageGlobals.picmip==2 && imageGlobals.picmipBump==1 && imageGlobals.picmipSpec==3);
 reflection.current.enabled=true;R_SetPicmip();assert(imageGlobals.picmip==2 && imageGlobals.picmipBump==2 && imageGlobals.picmipSpec==2);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4ios-campaign-picmip-') as tmp:
 p=Path(tmp);src=p/'test.cpp';src.write_text(stubs+production+tests)
 for label,defs in [('sp-device',['KISAK_SP','TARGET_OS_IPHONE=1','TARGET_OS_SIMULATOR=0','EXPECT_AUTO=1']),('mp-device',['KISAK_MP','TARGET_OS_IPHONE=1','TARGET_OS_SIMULATOR=0','EXPECT_AUTO=0']),('sp-simulator',['KISAK_SP','TARGET_OS_IPHONE=1','TARGET_OS_SIMULATOR=1','EXPECT_AUTO=0'])]:
  subprocess.run(['clang++','-std=c++17','-UTARGET_OS_IPHONE','-UTARGET_OS_SIMULATOR',*[f'-D{x}' for x in defs],str(src),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True);print(label+': passed')

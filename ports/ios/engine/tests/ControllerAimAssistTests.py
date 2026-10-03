#!/usr/bin/env python3
"""Verify production SP/MP gamepad slowdown and tracking with valid targets."""
from pathlib import Path
import subprocess, tempfile
repo=Path(__file__).resolve().parents[4]
s=(repo/'src/aim_assist/aim_assist.cpp').read_text()
bounds=s[s.index('static bool __cdecl AimAssist_DoBoundsIntersectCenterBox('):s.index('void __cdecl AimAssist_Setup(')]
best=s[s.index('const AimScreenTarget *__cdecl AimAssist_GetBestTarget('):s.index('const AimScreenTarget *__cdecl AimAssist_GetTargetFromEntity(')]
update=s[s.index('void AimAssist_UpdateGamepadInput('):s.index('void __cdecl AimAssist_DrawDebugOverlay(')]
register=''
for name in ('aim_slowdown_enabled','aim_lockon_enabled'):
    a=s.rfind('#ifdef KISAK_MP',0,s.index('    '+name+' = Dvar_RegisterBool('))
    b=s.index('\n    '+name.replace('_enabled','_debug'),a)
    register+=s[a:b]+'\n'
stubs=r'''
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#define __cdecl
#define iassert(x) assert(x)
constexpr int PM_NORMAL=0,PM_NORMAL_LINKED=1,STAT_HEALTH=0,DVAR_CHEAT=128;
struct Dvar {struct {bool enabled=true;float value=0;} current;};
Dvar slow,lock,scale;const Dvar *aim_slowdown_enabled,*aim_lockon_enabled;
const Dvar *aim_slowdown_pitch_scale=&scale,*aim_slowdown_yaw_scale=&scale,*aim_slowdown_pitch_scale_ads=&scale,*aim_slowdown_yaw_scale_ads=&scale,*aim_lockon_deflection=&scale,*aim_lockon_strength=&scale;
const Dvar *Dvar_RegisterBool(const char *name,bool value,int,const char*){Dvar &d=!strcmp(name,"aim_slowdown_enabled")?slow:lock;d.current.enabled=value;return &d;}
struct PS {int pm_type=PM_NORMAL,stats[1]={100};unsigned otherFlags=4;float fWeaponPosFrac=0,velocity[3]={};};
struct AimInput {int localClientNum=0;const PS *ps=nullptr;float deltaTime=1.f/60,pitch=0,yaw=0,pitchAxis=0,yawAxis=0,pitchMax=120,yawMax=180,forwardAxis=0,rightAxis=0;unsigned buttons=0;};
struct AimOutput {float pitch=0,yaw=0,meleeChargeYaw=0;int meleeChargeDist=0;};
struct AimScreenTarget {float clipMins[2]={-.01f,-.01f},clipMaxs[2]={.01f,.01f},aimPos[3]={100,0,0},velocity[3]={},distSqr=10000;};
struct AimAssistGlobals {bool initialized=true;float adsLerp=0,viewOrigin[3]={};int screenTargetCount=1;AimScreenTarget screenTargets[1];struct{float slowdownRegionWidth=.1f,slowdownRegionHeight=.1f,lockOnRegionWidth=.1f,lockOnRegionHeight=.1f;}tweakables;}aaGlobArray[1];
struct WeaponDef {float aimAssistRange=200,aimAssistRangeAds=200;} weapon;
struct {bool inKillCam=false;}cg;
auto CG_GetLocalClientGlobals(int){return &cg;}
unsigned AimAssist_GetWeaponIndex(int,const PS*){return 1;}
unsigned BG_GetNumWeapons(){return 2;}
const WeaponDef *BG_GetWeaponDef(unsigned){return &weapon;}
void Vec3Sub(const float *a,const float *b,float *o){for(int n=0;n<3;++n)o[n]=a[n]-b[n];}
void AimAssist_UpdateMouseInput(const AimInput *i,AimOutput *o){o->pitch=i->pitch;o->yaw=i->yaw;aaGlobArray[0].adsLerp=i->ps->fWeaponPosFrac;}
'''
tests=r'''
int main(){Register();assert(aim_slowdown_enabled->current.enabled && aim_lockon_enabled->current.enabled);
 scale.current.value=.5f;PS ps;AimInput i;i.ps=&ps;i.yawAxis=1;AimOutput o;
 AimAssist_UpdateGamepadInput(&i,&o,true);assert(std::fabs(o.yaw-1.5f)<.0001f);
 AimAssist_UpdateGamepadInput(&i,&o,false);assert(std::fabs(o.yaw-3.f)<.0001f);
 ps.fWeaponPosFrac=1;AimAssist_UpdateGamepadInput(&i,&o,true);assert(std::fabs(o.yaw-1.5f)<.0001f);
 aaGlobArray[0].screenTargets[0].clipMins[0]=.5f;AimAssist_UpdateGamepadInput(&i,&o,true);assert(std::fabs(o.yaw-3.f)<.0001f);
 aaGlobArray[0].screenTargets[0].clipMins[0]=-.01f;weapon.aimAssistRangeAds=50;
 AimAssist_UpdateGamepadInput(&i,&o,true);assert(std::fabs(o.yaw-3.f)<.0001f);weapon.aimAssistRangeAds=200;
 i.yawAxis=0;i.forwardAxis=1;aaGlobArray[0].screenTargets[0].velocity[1]=30;
 AimAssist_UpdateGamepadInput(&i,&o,true);assert(o.yaw>0 && o.yaw<.5f);
 i.forwardAxis=0;AimAssist_UpdateGamepadInput(&i,&o,true);assert(o.yaw==0);
 i.forwardAxis=1;ps.stats[0]=0;AimAssist_UpdateGamepadInput(&i,&o,true);assert(o.yaw==0);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4ios-aimassist-') as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+'void Register(){\n'+register+'}\n'+bounds+best+update+tests)
 for mode in ('KISAK_SP','KISAK_MP'):
  subprocess.run(['clang++','-std=c++17','-D'+mode,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
  print(mode+': default enabled, hip/ADS slowdown, tracking, range/cone/dead/disabled guards passed')

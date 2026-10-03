#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess
repo=Path(__file__).resolve().parents[4];s=(repo/'src/cgame_mp/cg_predict_mp.cpp').read_text();fn=s[s.index('void __cdecl CG_InterpolatePlayerState('):]
stubs=r'''
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
#define __cdecl
#define iassert assert
constexpr int STAT_SPAWN_COUNT=0,ENTITYNUM_NONE=1023;
struct playerState_s{int pm_type=0,clientNum=0,stats[1]{},deltaTime=0,eFlags=0,otherFlags=4,bobCycle=0;float aimSpreadScale=0,origin[3]{},velocity[3]{},viewangles[3]{},delta_angles[3]{},viewHeightCurrent=60,leanf=0,fWeaponPosFrac=0;int cursorHint=0,cursorHintString=0,cursorHintEntIndex=0;};
struct snapshot_s{playerState_s ps;int serverTime=0;};struct usercmd_s{};
struct cg_s{playerState_s predictedPlayerState;snapshot_s *snap=nullptr,*nextSnap=nullptr;float frameInterpolation=.5f;}cg;
cg_s *CG_GetLocalClientGlobals(int){return &cg;}int CL_GetCurrentCmdNumber(int){return 0;}
int CL_GetUserCmd(int,int,usercmd_s*){return 1;}void PM_UpdateViewAngles(playerState_s*,double,usercmd_s*,int){}
void Vec3Lerp(const float *a,const float *b,float f,float *o){for(int i=0;i<3;++i)o[i]=a[i]+f*(b[i]-a[i]);}
'''
tests=r'''
int main(){snapshot_s a{},b{};a.serverTime=100;b.serverTime=150;a.ps.origin[2]=1000;b.ps.origin[2]=100;cg.snap=&a;cg.nextSnap=&b;
 CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==550);
 a.ps.pm_type=5;CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==100);a.ps.pm_type=0;
 a.ps.otherFlags=0;CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==100);a.ps.otherFlags=4;
 ++b.ps.stats[0];CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==100);--b.ps.stats[0];
 a.ps.deltaTime=1000;CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==100);a.ps.deltaTime=0;
 b.ps.eFlags=2;CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==100);b.ps.eFlags=0;
 b.ps.clientNum=1;CG_InterpolatePlayerState(0,0);assert(cg.predictedPlayerState.origin[2]==100);
 puts("Production camera interpolation: normal movement blends; spectator, spawn, killcam, teleport and client transitions snap to server origin");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+fn+tests)
 subprocess.run(['clang++','-I'+str(repo/'ports/ios/engine'),'-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

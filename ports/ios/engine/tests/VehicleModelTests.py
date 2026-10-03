#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[4];s=(repo/'src/cgame_mp/cg_pose_mp.cpp').read_text();fn=s[s.index('void CG_VehPoseControllers('):s.index('\nvoid __cdecl CG_DoControllers')]
stubs=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
struct DObjAnimMat{float quat[4],trans[3],weight;};struct XModel{DObjAnimMat *pose;unsigned count;};struct DObj_s{XModel *model;};
struct cpose_t{int eType=0;struct{int pitch=0,roll=0,yaw=0,barrelPitch=0,steerYaw=0;int tag_body=0,tag_turret=0,tag_barrel=0;float time=0;unsigned wheelBoneIndex[4]{};uint16_t wheelFraction[4]{};}vehicle;struct{float barrelPitch=0;}turret;};
constexpr int ET_HELICOPTER=1;float vec3_origin[3]{};int count=0,bones[4];float offsets[4],steers[4];
void DObjSetLocalTag(DObj_s*,int32_t*,int,const float*,const float*){}
const XModel *DObjGetModel(const DObj_s *o,int){return o->model;}
unsigned XModelNumBones(const XModel *m){return m->count;}
const DObjAnimMat *XModelGetBasePose(const XModel *m){return m->pose;}
bool DObjSetRotTransIndex(DObj_s*,int32_t*,unsigned){return true;}
void DObjSetLocalTagInternal(const DObj_s*,const float *off,const float *angles,unsigned bone){assert(count<4);assert(off[0]==0 && off[1]==0);bones[count]=bone;offsets[count]=off[2];steers[count]=angles?angles[1]:0;++count;}
'''
tests=r'''
int main(){auto *pose=(DObjAnimMat*)calloc(4,sizeof(DObjAnimMat));XModel model{pose,4};DObj_s obj{&model};cpose_t state;
 state.vehicle.time=20;state.vehicle.steerYaw=16384;state.vehicle.wheelBoneIndex[0]=0;state.vehicle.wheelBoneIndex[1]=3;
 state.vehicle.wheelBoneIndex[2]=4;state.vehicle.wheelBoneIndex[3]=254;state.vehicle.wheelFraction[0]=65535;state.vehicle.wheelFraction[1]=0;
 CG_VehPoseControllers(&state,&obj,nullptr);assert(count==2 && bones[0]==0 && bones[1]==3);assert(offsets[0]==-20 && offsets[1]==20);assert(steers[0]==90 && steers[1]==90);
 model.pose=nullptr;CG_VehPoseControllers(&state,&obj,nullptr);assert(count==2);obj.model=nullptr;CG_VehPoseControllers(&state,&obj,nullptr);assert(count==2);free(pose);
 puts("Production MP vehicle controller: native model pointers, missing/bounded bones and suspension/steering passed ASan/UBSan");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+fn+tests)
 subprocess.run(['clang++','-I'+str(repo/'ports/ios/engine'),'-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

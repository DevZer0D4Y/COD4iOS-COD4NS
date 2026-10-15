#!/usr/bin/env python3
"""Exercise the SP vehicle controller used by campaign tanks and trucks."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4]
s=(r/'src/cgame/cg_pose.cpp').read_text()
a=s.index('void CG_Vehicle_DoControllers(');production=s[a:s.index('\nvoid __cdecl CG_Actor_DoControllers',a)]
stubs=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define iassert(x) assert(x)
struct DObjAnimMat{float quat[4],trans[3],weight;};
struct XModel{DObjAnimMat *pose;unsigned count;};struct DObj_s{XModel *model;};
struct cpose_t{float angles[3]{},origin[3]{};struct{float height=20;}actor;struct{int pitch=0,roll=0,yaw=0,barrelPitch=0,steerYaw=16384;int tag_body=0,tag_turret=0,tag_barrel=0;unsigned wheelBoneIndex[6]{0,3,4,254,253,255};uint16_t wheelFraction[6]{65535,0,0,0,0,0};}vehicle;};
float vec3_origin[3]{};int count=0;unsigned bones[6];float z[6],yaw[6];
void AnglesToAxis(const float*,float a[3][3]){memset(a,0,9*sizeof(float));for(int i=0;i<3;++i)a[i][i]=1;}
void DObjSetLocalTag(DObj_s*,int*,int,const float*,const float*){}
const XModel *DObjGetModel(const DObj_s *o,int){return o->model;}
unsigned XModelNumBones(const XModel *m){return m->count;}
const DObjAnimMat *XModelGetBasePose(const XModel *m){return m->pose;}
bool DObjSetRotTransIndex(DObj_s*,int*,unsigned){return true;}
void DObjSetLocalTagInternal(const DObj_s*,const float *offset,const float *angles,unsigned bone){assert(count<6);assert(fabs(offset[0])<.001 && fabs(offset[1])<.001);bones[count]=bone;z[count]=offset[2];yaw[count]=angles?angles[1]:0;++count;}
'''
tests=r'''
int main(){auto *base=(DObjAnimMat*)calloc(4,sizeof(DObjAnimMat));XModel model{base,4};DObj_s object{&model};cpose_t pose;
 CG_Vehicle_DoControllers(&pose,&object,nullptr);
 assert(count==2 && bones[0]==0 && bones[1]==3 && fabs(z[0]+20)<.01 && fabs(z[1]-20)<.01 && fabs(yaw[0]-90)<.01);
 model.pose=nullptr;CG_Vehicle_DoControllers(&pose,&object,nullptr);assert(count==2);
 object.model=nullptr;CG_Vehicle_DoControllers(&pose,&object,nullptr);assert(count==2);free(base);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-sp-vehicle-') as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Campaign vehicle pose: missing models/poses and out-of-range wheel bones rejected; six-wheel suspension and steering preserved.')

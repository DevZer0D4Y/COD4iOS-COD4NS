#!/usr/bin/env python3
"""Run the production brush initializer with the real pointer-bearing union layout."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[4]
s = (root/'src/game/actor_badplace.cpp').read_text()
start = s.index('void __cdecl Path_MakeBrushBadPlace(')
function = s[start:s.index('\n}\n', start)+3]
h = (root/'src/game/actor_badplace.h').read_text()
layouts = h[h.index('struct badplace_arc_t'):h.index('struct badplace_t')]
code = r'''#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <initializer_list>
#define __cdecl
#define FL_BADPLACE_VOLUME 0x8000
struct gentity_s {int flags;struct {float mins[3],maxs[3];}r;};
void MyAssertHandler(const char*,int,int,const char*,const char*){assert(false);}
float RadiusFromBounds2D(const float *a,const float *b){return std::hypot(b[0]-a[0],b[1]-a[1]);}
''' + layouts + r'''
badplace_parms_t captured;
void Path_MakeBadPlace(unsigned name,int duration,int teams,int type,badplace_parms_t *p){assert(name==7 && duration==250 && teams==6 && type==2);captured=*p;}
''' + function + r'''
int main(){
 static_assert(sizeof(void*)==8,"Regression must run with 64-bit pointers");
 for(float radius:{341.6961975f,1.f,8192.f}){
  gentity_s entity{};entity.r.maxs[0]=radius;
  Path_MakeBrushBadPlace(7,250,6,&entity);
  assert(captured.brush.volume==&entity);
  assert(captured.brush.radius==radius);
  assert(entity.flags & FL_BADPLACE_VOLUME);
  // Reproduce the reported corruption: float 0x43aad91d at byte 4.
  badplace_parms_t old{};old.brush.volume=&entity;old.arc.origin[1]=radius;
  assert(old.brush.volume!=&entity);
 }
 // Build 12 log: bad pointer 0x43aad91d0604b808, fault at pointer + 0xec.
 uint32_t bits=0x43aad91d;float radius;memcpy(&radius,&bits,4);
 badplace_parms_t old{};old.brush.volume=reinterpret_cast<gentity_s*>(0x10604b808ULL);old.arc.origin[1]=radius;
 assert(reinterpret_cast<uintptr_t>(old.brush.volume)+0xec==0x43aad91d0604b8f4ULL);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-badplace-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 print('Production brush initializer preserves entity pointer/radius; old write reproduces exact build 12 fault address.')

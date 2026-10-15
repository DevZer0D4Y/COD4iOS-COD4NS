#!/usr/bin/env python3
"""Run production UP upload path with a frame-slot ownership assertion."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4];s=(r/'ports/ios/d3d9/metal/metal_backend.mm').read_text()
a=s.index('void DrawUserPrimitives(');production=s[a:s.index('\nvoid Clear(',a)]
code=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <array>
struct FakeBuffer {size_t length=4096;};
struct Allocation {FakeBuffer buffer;size_t offset=0;void *pointer=nullptr;};
struct Buffer {FakeBuffer buffer;size_t length=0;};
struct StreamBinding {Buffer *buffer=nullptr;uint32_t offset=0,stride=0;};
struct DrawState {std::array<StreamBinding,4> streams;};enum class Primitive {Triangles};
bool acquired=false,failCommand=false,failAllocation=false;int allocations=0,draws=0;unsigned char bytes[4096];
struct Arena {Allocation Allocate(int,size_t n){assert(acquired);++allocations;assert(n<=sizeof(bytes));return {{},0,failAllocation?nullptr:bytes};}};
struct Context {std::recursive_mutex lock;Arena arena;int device=0;}context;Context *g=&context;
void *CommandBuffer(){if(failCommand)return nullptr;acquired=true;return &context;}
uint32_t VertexCount(Primitive,uint32_t n){return n*3;}
void Draw(const DrawState &s,Primitive,uint32_t,bool indexed,int32_t,uint32_t){assert(acquired && !indexed && s.streams[0].buffer && s.streams[0].stride==4);++draws;}
'''+production+r'''
int main(){DrawState state;unsigned char input[12];memset(input,17,sizeof(input));
 DrawUserPrimitives(state,Primitive::Triangles,1,input,4);assert(draws==1 && allocations==1 && !memcmp(input,bytes,12));
 acquired=false;failCommand=true;DrawUserPrimitives(state,Primitive::Triangles,1,input,4);assert(draws==1 && allocations==1);
 failCommand=false;failAllocation=true;DrawUserPrimitives(state,Primitive::Triangles,1,input,4);assert(draws==1 && allocations==2);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-metal-up-order-') as d:
 p=Path(d);(p/'test.mm').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fobjc-arc','-framework','Foundation','-fsanitize=address,undefined',str(p/'test.mm'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production UP draws acquire the frame slot before CPU writes; command/allocation failures skip draws without copying to null.')

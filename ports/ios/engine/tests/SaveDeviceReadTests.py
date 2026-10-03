#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4]
s=(r/'src/game/savedevice_pc.cpp').read_text()
production=s[s.index('\nint __cdecl ReadFromDevice(void *buffer')+1:s.index('static bool SaveExistsValidated(')]
stubs=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#define __cdecl
std::vector<unsigned char> file(10000,42);unsigned position=0;
unsigned FS_Read(unsigned char *b,unsigned n,int){n=std::min(n,unsigned(file.size())-position);memcpy(b,file.data()+position,n);position+=n;return n;}
'''
tests=r'''
int main(){unsigned char bytes[10];void *f=(void*)(intptr_t)1;
 assert(ReadFromDevice(nullptr,8000,f)==8000 && position==8000);
 assert(ReadFromDevice(bytes,10,f)==10 && position==8010 && bytes[0]==42);
 assert(ReadFromDevice(nullptr,3000,f)==1990 && position==10000);
 assert(ReadFromDevice(nullptr,0,f)==0 && ReadFromDevice(bytes,1,nullptr)==0);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Save source skipping consumes bytes, following records align, short reads reported')

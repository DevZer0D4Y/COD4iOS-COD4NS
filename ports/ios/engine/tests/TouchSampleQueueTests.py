#!/usr/bin/env python3
from pathlib import Path
import subprocess, tempfile
r=Path(__file__).resolve().parents[4]
s=(r/'ports/ios/engine/controller_input.cpp').read_text()
production=s[s.index('void KisakApple_ControllerSubmit('):s.index('// Registered from CL_InitOnceForAllClients')]
stubs=r'''
#include "controller_input.h"
#include <deque>
#include <mutex>
#include <cassert>
using namespace kisak::controller;
std::mutex inputMutex;std::deque<Snapshot> pending;
'''
tests=r'''
int main() {
 Snapshot s;s.connected=true;s.touch=true;s.lookDeltaX=12;s.lookDeltaY=4;
 KisakApple_ControllerSubmit(s);s.lookDeltaX=s.lookDeltaY=0;
 KisakApple_ControllerSubmit(s);assert(pending.size()==1 && pending.back().lookDeltaX==12);
 s.lookDeltaX=3;s.lookDeltaY=-1;KisakApple_ControllerSubmit(s);
 assert(pending.back().lookDeltaX==15 && pending.back().lookDeltaY==3);
 s.buttons=1u<<South;s.lookDeltaX=2;KisakApple_ControllerSubmit(s);
 assert(pending.size()==2 && pending.front().lookDeltaX==15 && pending.back().lookDeltaX==2);
 s.buttons=0;s.lookDeltaX=0;KisakApple_ControllerSubmit(s);assert(pending.size()==3);
 s.touch=false;s.rightX=.5;KisakApple_ControllerSubmit(s);
 assert(pending.size()==4 && !pending.back().touch && pending.back().lookDeltaX==0);
 s.rightX=.7;KisakApple_ControllerSubmit(s);assert(pending.size()==4 && pending.back().rightX==.7f);
 pending.clear();s.touch=true;s.buttons=0;
 for(int i=0;i<1200;++i) {s.lookDeltaX=(i%2)?0:1;KisakApple_ControllerSubmit(s);}
 assert(pending.size()==1 && pending.back().lookDeltaX==600);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17','-I'+str(r/'ports/ios/engine'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production queue: swipe increments survive zero-motion polling, merging and button/source transitions')

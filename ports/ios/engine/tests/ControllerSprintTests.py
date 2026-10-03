#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess
repo=Path(__file__).resolve().parents[4];s=(repo/'ports/ios/engine/controller_input.cpp').read_text()
routing=s[s.index('Route RouteFor('):s.index('void Emit(Route route')]+s[s.index('void ToggleScores() {'):s.index('void Process(unsigned time) {')]
process=s[s.index('void Process(unsigned time) {'):s.index('\n}\n}\n\nvoid KisakApple_ControllerSubmit')+2]
stubs=r'''
#include "controller_input.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
using namespace kisak::controller;
struct Route{const char *command=nullptr;int key=0;};
struct Dvar{struct{bool enabled=true;}current;};Dvar enabled,*gpadEnabled=&enabled;
Snapshot current;ButtonState buttonState;Route heldRoutes[Count];bool sprintRequested=false,menu=false;
int repeatButton=-1;unsigned repeatAt=0;std::vector<std::string> emitted;
#define KISAK_MP
constexpr int UIMENU_SCOREBOARD=10;
enum {K_ESCAPE=1,K_MOUSE1,K_ENTER,K_TAB,K_LEFTARROW,K_RIGHTARROW,K_PGUP,K_PGDN,K_UPARROW,K_DOWNARROW};
bool pointerMenu=false;const char *commands[Count]={};int activeMenu=0,opened=0,closed=0;
struct cg_s{void *nextSnap=(void*)1;bool showScores=false;}cg;
cg_s *CG_GetLocalClientGlobals(int){return &cg;}
int UI_GetActiveMenu(int){return activeMenu;}
void CG_ScoresDown_f(){cg.showScores=true;menu=true;activeMenu=10;++opened;}
void CG_ScoresUp_f(){cg.showScores=false;menu=false;activeMenu=0;++closed;}
bool InMenu(){return menu;}uint32_t EffectiveButtons(bool){return current.buttons;}
int I_stricmp(const char *a,const char *b){return strcmp(a,b);}
void Emit(Route r,int,bool down,unsigned){if(r.command)emitted.push_back(std::string(down?"+":"-")+r.command+1);}
'''
# Fix the deliberately simple stub expression to produce '+holdbreath'/'-holdbreath'.
stubs=stubs.replace('std::string(down?"+":"-")+r.command+1','std::string(down?"+":"-")+(r.command+1)')
tests=r'''
int main(){commands[L3]="+breath_sprint";commands[Options]="+scores";current.connected=true;Process(1);
 current.buttons=1u<<L3;Process(2);assert(sprintRequested);sprintRequested=false;
 for(int t=3;t<90;++t){Process(t);assert(!sprintRequested);}
 current.buttons=0;Process(90);assert(!sprintRequested);
 assert(emitted.size()==2 && emitted[0]=="+holdbreath" && emitted[1]=="-holdbreath");
 current.buttons=1u<<L3;Process(91);assert(sprintRequested);
 menu=true;Process(92);assert(!sprintRequested);
 menu=false;Process(93);assert(!sprintRequested);
 current.buttons=0;Process(94);current.buttons=1u<<L3;Process(95);assert(sprintRequested);
 current.connected=false;Process(96);assert(!sprintRequested);
 current.connected=true;current.buttons=0;Process(200);
 current.buttons=1u<<Options;Process(201);assert(cg.showScores && opened==1 && closed==0);
 for(int t=202;t<220;++t)Process(t);assert(cg.showScores && closed==0);
 current.buttons=0;Process(220);assert(cg.showScores && closed==0);
 current.buttons=1u<<Options;Process(221);assert(!cg.showScores && closed==1);
 current.buttons=0;Process(222);current.buttons=1u<<Options;Process(223);assert(cg.showScores && opened==2);
 current.buttons=0;Process(224);current.buttons=1u<<Options;Process(225);assert(!cg.showScores && closed==2);
 puts("Production controller routing: scoreboard click opens, release/hold stays open, next click closes; sprint/breath/pause/reconnect passed");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+routing+process+tests)
 subprocess.run(['clang++','-I'+str(repo/'ports/ios/engine'),'-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

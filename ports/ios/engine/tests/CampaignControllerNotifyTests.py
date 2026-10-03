#!/usr/bin/env python3
"""Exercise controller sprint against the actual SP command notification path."""
from pathlib import Path
import subprocess, tempfile
repo = Path(__file__).resolve().parents[4]
controller = (repo/'ports/ios/engine/controller_input.cpp').read_text()
process = controller[controller.index('void Process(unsigned time) {'):controller.index('\n}\n}\n\nvoid KisakApple_ControllerSubmit')+2]
cmd = (repo/'src/qcommon/cmd.cpp').read_text()
notify = cmd[cmd.index('void Cmd_CheckNotify()\n{'):cmd.index('void Cmd_LoadNotifications(')]
stubs = r'''
#include "controller_input.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>
#define KISAK_SP
#define iassert(x) assert(x)
using namespace kisak::controller;
struct Route {const char *command=nullptr;int key=0;};
struct Dvar {struct {bool enabled=true;int integer=0;} current;};
Dvar enabled,paused;Dvar *gpadEnabled=&enabled,*cl_paused=&paused;
Snapshot current;ButtonState buttonState;Route heldRoutes[Count];
bool sprintRequested=false,menu=false;int repeatButton=-1;unsigned repeatAt=0;
std::vector<std::string> args={"unrelated_console_command"}, captured, emitted;
std::vector<std::vector<std::string>> contexts;
const char *sprintCommand="+breath_sprint";
struct {uint16_t command,notify;} cmd_notify[]={{1,99},{2,99}};
int cmd_notifyCount=2;
bool Sys_IsMainThread(){return true;}
const char *Cmd_Argv(int n){return args.at(n).c_str();}
uint32_t SL_FindLowercaseString(const char *s){return !strcmp(s,"+breath_sprint")?1:!strcmp(s,"+sprint")?2:0;}
void G_AddCommandNotify(uint16_t n){assert(n==99 && args.size()==1);captured.push_back(Cmd_Argv(0));}
void Cmd_TokenizeString(char *s){contexts.push_back(args);args={s};}
void Cmd_EndTokenizedString(){args=contexts.back();contexts.pop_back();}
bool InMenu(){return menu;}
uint32_t EffectiveButtons(bool){return current.buttons;}
int I_stricmp(const char *a,const char *b){return strcmp(a,b);}
Route RouteFor(int,bool inMenu){return inMenu?Route{nullptr,13}:Route{sprintCommand,0};}
void ToggleScores(){}
void Emit(Route r,int,bool down,unsigned){if(r.command)emitted.push_back(std::string(down?"+":"-")+(r.command+1));}
'''
tests = r'''
int main(){
 current.connected=true;Process(1);
 current.buttons=1u<<L3;Process(2);assert(sprintRequested && captured.size()==1 && captured[0]=="+breath_sprint");
 assert(args[0]=="unrelated_console_command" && contexts.empty());sprintRequested=false;
 for(unsigned t=3;t<50;++t){Process(t);assert(!sprintRequested && captured.size()==1);}
 current.buttons=0;Process(50);assert(captured.size()==1 && emitted.size()==2);
 assert(emitted[0]=="+holdbreath" && emitted[1]=="-holdbreath");
 sprintCommand="+sprint";current.buttons=1u<<L3;Process(51);
 assert(sprintRequested && captured.size()==2 && captured[1]=="+sprint");
 sprintRequested=false;Process(52);assert(!sprintRequested && captured.size()==2);
 current.buttons=0;Process(53);menu=true;current.buttons=1u<<L3;Process(54);assert(captured.size()==2);
 current.buttons=0;Process(55);menu=false;paused.current.integer=1;current.buttons=1u<<L3;Process(56);assert(captured.size()==2);
 current.connected=false;Process(57);assert(!sprintRequested);
 assert(args[0]=="unrelated_console_command" && contexts.empty());
 paused.current.integer=0;enabled.current.enabled=false;current.connected=true;current.buttons=0;current.touch=true;Process(60);
 current.buttons=1u<<L3;Process(61);assert(sprintRequested && captured.size()==3);
 current.buttons=0;Process(62);current.touch=false;Process(63);current.buttons=1u<<L3;Process(64);
 assert(!sprintRequested && captured.size()==3);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4ios-training-notify-') as d:
    p=Path(d);(p/'test.cpp').write_text(stubs+notify+process+tests)
    subprocess.run(['clang++','-std=c++17','-I'+str(repo/'ports/ios/engine'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('SP training: one original sprint notification per click, argument context restored, hold/release/menu/pause/disconnect passed')

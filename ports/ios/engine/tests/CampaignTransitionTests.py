#!/usr/bin/env python3
"""Exercise production transition and checkpoint selection with fault injection."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4]
def function(path,signature):
 s=(r/path).read_text();a=s.index(signature);b=s.index('\n}\n',a)+3;return s[a:b]
production=function('src/game/g_scr_main.cpp','void GScr_ChangeLevel()')+function('src/game/g_save.cpp','void __cdecl G_PrepareSaveMemoryForWrite(')+function('src/game/savememory.cpp','int __cdecl SaveMemory_WriteSaveToDevice(')
stubs=r'''
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <limits>
#define __cdecl
#define SAVE_GAME_HANDLE 0
#define CON_CHANNEL_FILES 0
#define iassert(x) assert(x)
struct Dvar {struct {int integer=0;float value=0;}current;};
Dvar reload,delay;Dvar *g_reloading=&reload,*g_changelevel_time=&delay;
struct gentity_s {int health=100,classname=0;} entity;gentity_s g_entities[1];
struct {int player=0;}scr_const;
struct {int exitTime=123,savepersist=123,changelevel=0;}level;
bool havePlayer=true;unsigned count=1;double seconds=0;int persistent=0,errors=0;std::string next;
gentity_s *G_Find(int,size_t,int){return havePlayer?&entity:nullptr;}
void GScr_TraceLevelEndCall(const char *){}
unsigned Scr_GetNumParam(){return count;}
float Scr_GetFloat(unsigned i){assert(i==2);return seconds;}
int Scr_GetInt(unsigned i){assert(i==1);return persistent;}
const char *Scr_GetString(unsigned i){assert(i==0);return next.c_str();}
void Scr_Error(const char *){++errors;}
void Scr_ParamError(unsigned i,const char *){assert(i==2);++errors;}
void G_SetNextMap(const char *){}
enum SaveBufferState {MEMCLEAR,AWAITING_COMMIT,COMMITTED};
struct SaveGame {SaveBufferState saveState;bool isWrittenToDevice=false,suppressPlayerNotify=false;struct {int bufferSize=10;unsigned char *buffer=(unsigned char*)1;}memFile;struct {int bodySize=10;char filename[64]="checkpoint";}header;};
SaveGame first{AWAITING_COMMIT},old{COMMITTED};SaveGame *current=&first,*committed=&old;
int writes=0,writeResult=0,finalizes=0,lastNames=0;
SaveGame *SaveMemory_GetSaveHandle(int){return current;}
SaveGame *SaveMemory_GetLastCommittedSave(){return committed;}
bool SaveMemory_IsWaitingForCommit(SaveGame *s){return s->saveState==AWAITING_COMMIT;}
void SaveMemory_ForceCommitSave(SaveGame *s){assert(s==current);auto prior=committed;committed=current;current=prior;committed->saveState=COMMITTED;}
bool SaveMemory_IsCurrentCommittedSaveValid(){return committed->saveState==COMMITTED;}
bool SaveMemory_IsWrittenToDevice(SaveGame *s){return s->isWrittenToDevice;}
void G_WriteCurrentCommitToDevice(){++writes;committed->isWrittenToDevice=true;}
void MyAssertHandler(const char *,int,int,const char *,...){assert(false);}
void Com_Printf(int,const char *,...){}
int Sys_Milliseconds(){return 0;}
int WriteSaveToDevice(unsigned char *,const void *,bool){return writeResult;}
bool SV_IsInternalSave(const char *){return false;}
void SV_SetLastSaveName(const char *){++lastNames;}
void SaveMemory_FinalizeSaveCommit(SaveGame *s){++finalizes;s->saveState=COMMITTED;}
'''
tests=r'''
int main(){
 const char *maps[]={"killhouse","cargoship","coup","blackout","armada","bog_a","hunted","ac130","bog_b","airlift","aftermath","village_assault","scoutsniper","sniperescape","village_defend","ambush","icbm","launchfacility_a","launchfacility_b","jeepride","airplane"};
 for(auto map:maps){next=map;count=1;level={123,123,0};GScr_ChangeLevel();assert(level.changelevel==1 && level.savepersist==0 && level.exitTime==0);}
 count=2;persistent=1;GScr_ChangeLevel();assert(level.savepersist==1);
 count=3;seconds=1.125;GScr_ChangeLevel();assert(level.exitTime==1125);
 delay.current.value=4;GScr_ChangeLevel();assert(level.exitTime==4000);delay.current.value=0;
 for(double value:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),2147483.0}){seconds=value;level.changelevel=0;int before=errors;GScr_ChangeLevel();assert(errors==before+1 && !level.changelevel);}
 count=0;level.changelevel=0;GScr_ChangeLevel();assert(!level.changelevel);
 count=1;havePlayer=false;GScr_ChangeLevel();assert(!level.changelevel);havePlayer=true;
 // Before the fix old.isWrittenToDevice incorrectly suppressed this write.
 old.isWrittenToDevice=true;G_PrepareSaveMemoryForWrite(6);
 assert(writes==1 && committed==&first && committed->isWrittenToDevice);
 G_PrepareSaveMemoryForWrite(4);assert(writes==1);
 SaveGame pending{AWAITING_COMMIT};writeResult=-1;
 assert(SaveMemory_WriteSaveToDevice(&pending)==-1 && pending.saveState==AWAITING_COMMIT && !pending.isWrittenToDevice && !finalizes && !lastNames);
 writeResult=0;assert(SaveMemory_WriteSaveToDevice(&pending)==0 && pending.isWrittenToDevice && finalizes==1 && lastNames==1);
 pending.saveState=MEMCLEAR;assert(SaveMemory_WriteSaveToDevice(&pending)==-1);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Campaign transitions and checkpoint buffer swap/failure retry regressions passed')

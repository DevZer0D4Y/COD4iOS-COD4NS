#!/usr/bin/env python3
"""Run the production map command against a case-sensitive save lookup."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4]
s=(r/'src/server/sv_ccmds.cpp').read_text()
production=s[s.index('void SV_Map_f()'):s.index('void SV_Map_f()')+s[s.index('void SV_Map_f()'):].index('\n}\n')+3]
stubs=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#define __int8 char
#define MAX_OSPATH 256
#define ERR_DROP 1
#define SAVE_ERROR_MISSING_DEVICE 1
int com_errorPrintsCount=0,errors=0,spawns=0;
struct Dvar {struct {bool enabled=false;} current;};
Dvar fast,cheats;Dvar *useFastFile=&fast,*sv_cheats=&cheats;
std::string arg,opened,pending,spawned;
bool loadAsSave=false,readOK=true;
void SV_Cmd_ArgvBuffer(int,char *p,int n){snprintf(p,n,"%s",arg.c_str());}
void I_strncpyz(char *p,const char *v,int n){snprintf(p,n,"%s",v);}
int I_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
void I_strlwr(char *p){while(*p){*p=tolower((unsigned char)*p);++p;}}
int ExtractMapStringFromSaveGame(const char *name,char *map){
 opened=name;if(!readOK)return 0;
 assert(opened=="profiles/TestUser/save/autosave/cargoship-2.svg" || opened=="profiles/TestUser/save/autosave/cargoship-2.SVG");
 strcpy(map,"CARGOSHIP");return 1;
}
void G_SaveError(int,int,const char *,...){++errors;}
void G_SetPendingLoadName(const char *s){pending=s;}
const char *Win_GetLanguage(){return "italian";}
void Com_sprintf(char *p,int n,const char *format,...){va_list args;va_start(args,format);vsnprintf(p,n,format,args);va_end(args);}
bool FS_SV_FileExists(const char *){return true;}
void Com_Error(int,const char *,...){++errors;}
void Dvar_SetBool(Dvar *d,bool v){d->current.enabled=v;}
void CL_ShutdownDemo(){}
void FS_ConvertPath(char *){}
void SV_SpawnServer(const char *s,bool save){spawned=s;loadAsSave=save;++spawns;}
void ShowLoadErrorsSummary(const char *,unsigned){}
'''
tests=r'''
int main(){
 arg="profiles/TestUser/save/autosave/cargoship-2.svg";SV_Map_f();
 assert(errors==0 && spawns==1 && spawned=="cargoship" && loadAsSave && pending==arg);
 arg="profiles/TestUser/save/autosave/cargoship-2.SVG";SV_Map_f();
 assert(errors==0 && spawns==2 && pending==arg);
 arg="KILLHOUSE";SV_Map_f();assert(spawned=="killhouse" && !loadAsSave && spawns==3);
 readOK=false;arg="profiles/TestUser/save/autosave/cargoship-2.svg";SV_Map_f();
 assert(errors==1 && spawns==3);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production map loader: profile case preserved, .SVG accepted, map identifiers normalized, failed reads abort spawn')

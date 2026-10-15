#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4];s=(r/'src/game/g_scr_main.cpp').read_text()
a=s.index('void __cdecl GScr_SetMissionDvar()');production=s[a:s.index('\nvoid GScr_Cinematic()',a)]
stubs=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <cstdarg>
#define __cdecl
#define CON_CHANNEL_SYSTEM 0
struct dvar_s{int type;};dvar_s unlocked{5},difficulty{7},other{5};
const char *missionDvarList[]={"mis_01","mis_difficulty",nullptr};
unsigned count=2;std::string name="mis_01",text="012340",actual;int value=8,result=0,errors=0,saves=0;
unsigned Scr_GetNumParam(){return count;}
const char *Scr_GetString(unsigned i){return i?text.c_str():name.c_str();}
int Scr_GetInt(unsigned i){assert(i==1);return value;}
const char *va(const char *s,...){return s;}
void Scr_Error(const char*){++errors;}
const dvar_s *Dvar_FindVar(const char *s){if(!strcmp(s,"mis_01"))return &unlocked;if(!strcmp(s,"mis_difficulty"))return &difficulty;if(!strcmp(s,"r_picmip"))return &other;return nullptr;}
void Dvar_SetIntByName(const char *s,int n){assert(!strcmp(s,"mis_01"));result=n;}
void Dvar_SetStringByName(const char *s,const char *v){assert(!strcmp(s,"mis_difficulty"));actual=v;}
const char *Dvar_GetVariantString(const char*){return "value";}
void Com_Printf(int,const char*,...){}
void Campaign_SaveProgress(){++saves;}
'''
tests=r'''
int main(){GScr_SetMissionDvar();assert(result==8 && errors==0 && saves==1);
 name="mis_difficulty";GScr_SetMissionDvar();assert(actual==text && saves==2);
 name="r_picmip";GScr_SetMissionDvar();assert(errors==1 && saves==2);
 name="mis_01";count=1;GScr_SetMissionDvar();assert(errors==2 && saves==2);
 count=2;unlocked.type=1;GScr_SetMissionDvar();assert(errors==3 && saves==2);}
'''
with tempfile.TemporaryDirectory(prefix='cod4-mission-dvars-') as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production mission setter updates both allowed variables, persists them and rejects unrelated variables, wrong arity and wrong types.')

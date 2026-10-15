#!/usr/bin/env python3
"""Exercise production mission registration, persistence and profile switching."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4];s=(r/'src/ui/mission.cpp').read_text()
a=s.index('const dvar_t *mis_cheat;');production=s[a:]
stubs=r'''
#include "ports/ios/save/CampaignProgress.h"
#include <cassert>
#include <filesystem>
#include <map>
#define DVAR_ARCHIVE 1
#define CON_CHANNEL_FILES 0
struct dvar_t {unsigned flags=0;struct {int integer=0;const char *string="";}current;std::string storage;};
std::map<std::string,dvar_t> dvars;std::string root;dvar_t profile,home;const dvar_t *com_playerProfile=&profile,*fs_homepath=&home;
struct SaveHeader {char before[32];char mapName[256];};
void Campaign_SaveProgress();
void Campaign_RestoreProgress();
int resumes=0;void SaveDevice_RestoreResume(){++resumes;}
void Dvar_SetInt(const dvar_t *d,int value){const_cast<dvar_t*>(d)->current.integer=value;}
void Dvar_SetBool(const dvar_t *d,int value){Dvar_SetInt(d,value);}
void Dvar_SetString(const dvar_t *d,const char *value){auto &v=*const_cast<dvar_t*>(d);v.storage=value;v.current.string=v.storage.c_str();}
const dvar_t *Dvar_RegisterInt(const char *name,int value,int,int,unsigned flags,const char*){auto &d=dvars[name];d.flags=flags;Dvar_SetInt(&d,value);return &d;}
const dvar_t *Dvar_RegisterString(const char *name,const char *value,unsigned flags,const char*){auto &d=dvars[name];d.flags=flags;Dvar_SetString(&d,value);return &d;}
const dvar_t *Dvar_RegisterBool(const char *name,int value,unsigned flags,const char*){return Dvar_RegisterInt(name,value,0,1,flags,"");}
void FS_BuildOSPath(const char*,const char*,const char*,char *path){strcpy(path,root.c_str());}
void Com_PrintWarning(int,const char*,...){}
'''
tests=r'''
int main(){char directory[]="/tmp/cod4-profile-XXXXXX";assert(mkdtemp(directory));root=directory;
 std::filesystem::create_directories(root+"/profiles/Tester");std::filesystem::create_directories(root+"/profiles/Other");
 Campaign_RegisterDvars();assert((mis_01->flags&DVAR_ARCHIVE) && (mis_difficulty->flags&DVAR_ARCHIVE));
 Dvar_SetString(com_playerProfile,"Tester");Dvar_SetInt(mis_01,8);std::string difficulty(50,'0');difficulty[7]='3';Dvar_SetString(mis_difficulty,difficulty.c_str());Campaign_SaveProgress();
 Campaign_BeginProfile();assert(mis_01->current.integer==0);Campaign_RestoreProgress();assert(mis_01->current.integer==8 && mis_difficulty->current.string[7]=='3');
 Dvar_SetString(com_playerProfile,"Other");Campaign_BeginProfile();Campaign_RestoreProgress();assert(mis_01->current.integer==0 && mis_difficulty->current.string[7]=='0');
 Dvar_SetString(com_playerProfile,"Tester");Campaign_BeginProfile();Campaign_RestoreProgress();assert(mis_01->current.integer==8);
 // Initial startup chooses a profile before client/server dvar registration.
 mis_01=nullptr;mis_difficulty=nullptr;mis_cheat=nullptr;int before=resumes;Campaign_RegisterDvars();assert(mis_01->current.integer==8 && resumes==before+1);
 std::filesystem::remove_all(directory);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-profile-test-') as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I',str(r),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production mission registration archives unlock/difficulty values; restoration and profile switching preserve earned progress without cross-profile leakage.')

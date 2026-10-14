#!/usr/bin/env python3
"""Run actual commit functions, then kill the process without a config write."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4]
def function(path,signature):
 s=(r/path).read_text();a=s.index(signature);return s[a:s.index('\n}\n',a)+3]
production=function('src/game/savememory.cpp','int __cdecl SaveMemory_CommitSave(')+function('src/game/g_save.cpp','int __cdecl G_ProcessCommitActions(')
stubs=r'''
#include "ports/ios/save/NativeSaveFile.h"
#include "ports/ios/save/SaveResume.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#define __cdecl
#define CON_CHANNEL_FILES 0
enum SaveBufferState {MEMCLEAR,AWAITING_COMMIT,COMMITTED};
struct Header {bool internalSave=false,demoPlayback=false;char filename[64]="profiles/Tester/save/autosave/blackout-inside.svg";};
struct SaveGame {SaveBufferState saveState=AWAITING_COMMIT;bool isUsingGlobalBuffer=true,isWrittenToDevice=false,suppressPlayerNotify=false;struct {char *buffer=(char*)1;}memFile;Header header;};
struct PendingSave {unsigned commitLevel=2;int saveType=0;};
SaveGame first,old;struct {SaveGame *currentGameSave=&first,*committedGameSave=&old;}saveMemoryGlob;
int writes=0,notifications=0,errors=0,lastNames=0;bool fail=false;std::string players;
void MyAssertHandler(const char*,int,int,const char*,...){assert(false);}
void Com_Printf(int,const char*,...){}
bool SV_IsInternalSave(const char*){return false;}
void SV_SetLastSaveName(const char*){++lastNames;}
void SV_GameSendServerCommand(int,const char*){++notifications;}
void SV_DisplaySaveErrorUI(){++errors;}
std::string fixture(){std::string bytes(1108+256,0);int v=287,n=256;memcpy(bytes.data(),&v,4);memcpy(bytes.data()+1104,&n,4);memset(bytes.data()+1108,char('a'+writes),n);auto crc=kisak::savefile::CRC(bytes.data()+1108,n);memcpy(bytes.data()+8,&crc,4);return bytes;}
int SaveMemory_WriteSaveToDevice(SaveGame *save){
 ++writes;if(fail)return -1;
 const auto data=fixture(),destination=players+"/"+save->header.filename,temporary=players+"/pending.svg";
 std::filesystem::create_directories(std::filesystem::path(destination).parent_path());
 std::ofstream(temporary,std::ios::binary).write(data.data(),data.size());
 assert(kisak::save::VerifyAndCommit(temporary.c_str(),destination.c_str(),data.data(),1108,data.data()+1108,256,data.size()));
 assert(kisak::savefile::RecordResume(players,"Tester",save->header.filename));
 save->isWrittenToDevice=true;return 0;
}
int SaveMemory_CommitSave(SaveGame*,int);
void SaveMemory_ForceCommitSave(SaveGame *save){assert(SaveMemory_CommitSave(save,0));}
bool SaveMemory_IsWrittenToDevice(SaveGame *save){return save->isWrittenToDevice;}
'''
tests=r'''
int main(){
 char folder[]="/tmp/cod4-commit-XXXXXX";assert(mkdtemp(folder));players=std::string(folder)+"/players";
 pid_t child=fork();assert(child>=0);
 if(!child){PendingSave request;assert(G_ProcessCommitActions(&request,&first)==0);assert(writes==1 && notifications==1 && first.isWrittenToDevice);_exit(0);}
 int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
 const std::string name=first.header.filename,path=players+"/"+name;
 assert(kisak::savefile::Complete(path.c_str()));assert(kisak::savefile::ResolveResume(players,"Tester","")==name);
 // A forced commit+write (level 6) must not rotate the same new save twice.
 PendingSave request;request.commitLevel=6;assert(G_ProcessCommitActions(&request,&first)==0);assert(writes==1);
 std::ifstream backup(path+".bak",std::ios::binary);std::string previous{std::istreambuf_iterator<char>(backup),{}};assert(!previous.empty());
 SaveGame &next=old;next.saveState=AWAITING_COMMIT;next.isWrittenToDevice=false;
 fail=true;assert(SaveMemory_CommitSave(&next,0)==1);assert(errors==1 && notifications==1 && !next.isWrittenToDevice);assert(kisak::savefile::Complete(path.c_str()));
 assert(kisak::savefile::ResolveResume(players,"Tester","")==name);
 // Deferred/unaccepted saves remain in memory until the script commits them.
 SaveGame pending;request.commitLevel=1;int before=writes;assert(G_ProcessCommitActions(&request,&pending)==0 && writes==before);
 std::filesystem::remove_all(folder);
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-checkpoint-commit-') as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+production+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I',str(r),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production RAM commit persists autosaves before abrupt exit; no duplicate disk rotation; failed writes preserve durable Resume.')

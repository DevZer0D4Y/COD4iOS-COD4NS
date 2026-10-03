#!/usr/bin/env python3
"""Regression checks for the production CoD4x redirect recovery path."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[4]
source = (repo/'src/client_mp/cl_main_mp.cpp').read_text()
function = source[source.index('void __cdecl CL_WWWDownload()'):source.index('void __cdecl CL_CheckForUpdateKeyAuth')]
source = (repo/'src/client_mp/cl_cod4x.cpp').read_text()
command = source[source.index('bool CL_CoD4xDownloadCommand('):source.index('bool CL_CoD4xVerifyDownload(')]
stubs = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>
#undef __APPLE__
#define __cdecl
constexpr int ERR_DROP=1, CON_CHANNEL_CLIENT=0;
enum dlStatus_t { DL_CONTINUE=0, DL_DONE=1, DL_FAILED=2 };
struct { int download=0; bool wwwDlDisconnected=false, wwwDlInProgress=true;
 char originalDownloadName[256]="mods/test/archive.iwd",downloadName[256]="archive.iwd",downloadTempName[256]="archive.tmp"; } cls;
struct { char cl_downloadName[64]="archive.iwd"; } legacyHacks;
struct { struct { const char *string="/tmp"; } current; } home, *fs_homepath=&home;
bool autoupdateStarted=false, valid=true, cod4x=true, started=true;
int promoted=0, removed=0, advanced=0, sentOpcode=-1;
dlStatus_t status=DL_DONE;
std::string reliable;
bool CL_UsesCoD4x(){return cod4x;}
void downloadMessage(int opcode){sentOpcode=opcode;}
int DL_DownloadLoop(){return status;}
bool DL_DLIsMotd(){return false;}
void CL_FinishMotdDownload(){}
bool CL_CoD4xVerifyDownload(const char *){return valid;}
void Com_PrintError(int,const char *,...){}
void Com_Printf(int,const char *,...){}
void Com_Error(int,const char *,...){throw std::runtime_error("drop");}
void FS_BuildOSPath(char *,char *,char *,char *out){strcpy(out,"destination/");}
void FS_CopyFile(const char *,const char *){++promoted;}
int testRename(const char *,const char *){++promoted;return 0;}
int testRemove(const char *){++removed;return 0;}
#define rename testRename
#define remove testRemove
void I_strncpyz(char *out,const char *in,int){strcpy(out,in);}
void Cbuf_AddText(int,const char *){}
void CL_AddReliableCommand(int,const char *text){reliable=text;}
void CL_NextDownload(int){++advanced;}
char *va(const char *,...){return const_cast<char*>("failed");}
void CL_ClearStaticDownload(){}
'''
tests=r'''
void reset(){cls={};cls.wwwDlInProgress=true;valid=true;cod4x=true;started=true;promoted=removed=advanced=0;sentOpcode=-1;reliable.clear();status=DL_DONE;}
int main(){
 reset();valid=false;CL_WWWDownload();
 assert(reliable=="wwwdl chkfail" && promoted==0 && advanced==0 && removed==1 && !cls.wwwDlInProgress);
 assert(CL_CoD4xDownloadCommand(reliable.c_str()) && sentOpcode==7);
 reset();valid=false;cls.wwwDlDisconnected=true;
 bool dropped=false;try{CL_WWWDownload();}catch(const std::runtime_error&){dropped=true;}
 assert(dropped && promoted==0 && advanced==0 && reliable.empty());
 reset();CL_WWWDownload();assert(promoted==1 && advanced==1 && reliable=="wwwdl done" && !cls.wwwDlInProgress);
 assert(CL_CoD4xDownloadCommand(reliable.c_str()) && sentOpcode==5);
 reset();status=DL_FAILED;CL_WWWDownload();assert(reliable=="wwwdl fail" && !promoted && !advanced);
 assert(CL_CoD4xDownloadCommand(reliable.c_str()) && sentOpcode==4);
 reset();cod4x=false;assert(!CL_CoD4xDownloadCommand("wwwdl chkfail") && sentOpcode==-1);
 puts("CoD4x downloads: checksum rejection retries same file through server; valid and disconnected paths preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='kisak-download-tests-') as tmp:
    cpp=Path(tmp)/'test.cpp'; exe=Path(tmp)/'test'
    cpp.write_text(stubs+command+function+tests)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

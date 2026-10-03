#!/usr/bin/env python3
"""Exercise production browser/cache code against empty Documents."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[4]
source=(root/'src/client_mp/cl_cod4x.cpp').read_text()
cache=source[source.index('using Verdict ='):source.index('void send(msg_t *msg)')]
start=source.index('void CL_CoD4xRecordServerError(')
error=source[start:source.index('\n}\n',start)+3]
stubs=r'''
#include "cod4x_server_filter.h"
#include "cod4x_verified_servers.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <vector>
#include <string>
#include <arpa/inet.h>
struct dvar_t {};
struct netadr_t {uint8_t ip[4];uint16_t port;};
constexpr int CA_CONNECTING=3, CON_CHANNEL_CLIENT=0;
struct {int connectionState=CA_CONNECTING;} clientUIActives[1];
struct {netadr_t serverAddress;} connection;
std::string documents;
const char *Dvar_GetString(const char *){return documents.c_str();}
void FS_BuildOSPath(const char *root,char *name,char *,char *out){snprintf(out,1024,"%s/%s",root,name);}
void I_strncpyz(char *out,const char *s,int n){snprintf(out,n,"%s",s);}
auto *CL_GetLocalClientConnection(int){return &connection;}
bool CL_UsesCoD4x(){return true;}
void Com_Printf(int,const char *,...){}
netadr_t address(unsigned a,unsigned b,unsigned c,unsigned d,unsigned port){
 return {{uint8_t(a),uint8_t(b),uint8_t(c),uint8_t(d)},htons(port)};
}
'''
tests=r'''
int main(int argc,char **argv){
 assert(argc==2);documents=argv[1];
 const auto apg=address(54,36,177,240,28930);
 assert(worthShowing(apg));assert(!worthShowing(address(163,176,199,150,28961)));
 assert(verdicts.empty());
 connection.serverAddress=apg;
 CL_CoD4xRecordServerError("Server asked for Steam authentication");
 assert(verdicts.empty() && worthShowing(apg));
 CL_CoD4xRecordServerError("Authorization failed to complete within the timeout limit.");
 assert(!worthShowing(apg) && verdicts.size()==1);
 verdicts.clear();verdictsLoaded=false;assert(!worthShowing(apg));
 recordVerdict(false);assert(worthShowing(apg));
 connection.serverAddress=address(203,0,113,99,28960);
 assert(!worthShowing(connection.serverAddress));
 recordVerdict(false);assert(worthShowing(connection.serverAddress));
 CL_CoD4xRecordServerError("Player kicked by scriptadmin");
 assert(worthShowing(connection.serverAddress));
 CL_CoD4xRecordServerError("Requires Steam authentication");
 assert(!worthShowing(connection.serverAddress));
 connection.serverAddress=apg;clientUIActives[0].connectionState=0;
 CL_CoD4xRecordServerError("Authorization failed to complete within the timeout limit.");
 assert(worthShowing(apg));
 puts("Production browser: empty Documents, persistent rejection, successful direct joins and non-auth errors passed");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(stubs+cache+error+tests)
    subprocess.run(['xcrun','clang++','-std=c++17','-I',str(root/'ports/ios/network'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test'),str(p)],check=True)

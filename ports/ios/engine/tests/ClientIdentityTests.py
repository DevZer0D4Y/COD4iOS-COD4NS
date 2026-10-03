#!/usr/bin/env python3
"""Exercise production identity creation and the non-Steam challenge expression."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[4]
source=(root/'src/client_mp/cl_main_mp.cpp').read_text()
start=source.index('const char *CL_GetClientGuid()')
end=source.index('\n}\n',start)+3
identity=source[start:end]
challenge=next(line.strip() for line in source.splitlines()
               if 'v2 = va(' in line and 'CL_GetClientGuid()' in line)
stubs=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <set>
#include <cctype>
constexpr int DVAR_ARCHIVE=1;
struct dvar_s {struct {const char *string;} current;};
using dvar_t=dvar_s;
std::string persisted;
dvar_s setting{{""}};
const dvar_t *Dvar_RegisterString(const char *,const char *,int,const char *) {return &setting;}
void Dvar_SetString(dvar_s *d,const char *v){persisted=v;d->current.string=persisted.c_str();}
const char *va(const char *fmt,...){static char out[128];va_list a;va_start(a,fmt);vsnprintf(out,sizeof(out),fmt,a);va_end(a);return out;}
'''
tests=r'''
int main(){
 std::set<std::string> installs;
 for(int i=0;i<1000;++i){
  persisted.clear();setting.current.string=persisted.c_str();
  std::string guid=CL_GetClientGuid();assert(guid.size()==32);
  for(unsigned char c:guid)assert(isxdigit(c));
  assert(installs.insert(guid).second);assert(guid==CL_GetClientGuid());
  assert(std::string(challenge())=="getchallenge 0 \""+guid+"\"");
 }
 persisted="abcdef0123456789abcdef0123456789";setting.current.string=persisted.c_str();
 assert(std::string(CL_GetClientGuid())==persisted);
 assert(std::string(challenge())=="getchallenge 0 \""+persisted+"\"");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)
    (p/'test.cpp').write_text(stubs+identity+'\nconst char *challenge(){'+challenge.replace('v2 =','return')+'}\n'+tests)
    subprocess.run(['xcrun','clang++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('Production identity: 1000 unique installs, stable saved GUID, upstream challenge bytes passed')

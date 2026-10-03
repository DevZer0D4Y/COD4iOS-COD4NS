#!/usr/bin/env python3
from pathlib import Path
import re, subprocess, tempfile
repo=Path(__file__).resolve().parents[4]
ui=(repo/'src/ui/ui_shared_obj.cpp').read_text(); header=(repo/'src/ui/ui_shared.h').read_text()
def fn(s,start,end):return s[s.index(start):s.index(end,s.index(start))]
types='\n'.join(fn(header,'struct '+name+' ', '\n};')+'\n};' for name in ['__declspec(align(8)) token_s','define_s'])
types=types.replace('__declspec(align(8))','alignas(8)')
stubs=r'''
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#define __cdecl
constexpr int ERR_FATAL=1;
void *Z_Malloc(size_t size,const char *,int){return malloc(size);}
void Z_Free(void *p,int){free(p);}
void Com_Error(int,const char *){abort();}
struct source_s{};
'''
functions=fn(ui,'uint32_t *__cdecl GetMemory(', '\nvoid __cdecl PC_FreeToken')+fn(ui,'uint8_t *__cdecl GetClearedMemory(', '\nvoid __cdecl PS_CreatePunctuationTable')+fn(ui,'define_s *__cdecl PC_CopyDefine(', '\ndefine_s *globaldefines;')
tests=r'''
int main(){
 token_s input{};strcpy(input.string,"weapon");input.floatvalue=42.125;input.line=77;input.linescrossed=3;
 token_s second{};strcpy(second.string,"two");input.next=&second;
 auto *copy=PC_CopyToken(&input);
 assert(reinterpret_cast<uintptr_t>(copy)%alignof(token_s)==0);
 assert(copy->floatvalue==42.125 && copy->line==77 && copy->linescrossed==3 && !copy->next);
 define_s d{};char name[]="CUSTOM_CLASS";d.name=name;d.flags=17;d.numparms=1;d.tokens=&input;d.parms=&second;
 auto *cloned=PC_CopyDefine(nullptr,&d);
 assert(!strcmp(cloned->name,name) && cloned->flags==17 && cloned->numparms==1);
 assert(cloned->tokens!=&input && cloned->tokens->next!=&second);
 assert(!strcmp(cloned->tokens->next->string,"two") && !cloned->tokens->next->next);
 assert(cloned->parms!=&second && !cloned->parms->next);
 for(token_s *t=cloned->tokens;t;){auto *next=t->next;FreeMemory((char*)t);t=next;}
 FreeMemory((char*)cloned->parms);FreeMemory((char*)cloned);FreeMemory((char*)copy);
 auto **hash=(define_s**)GetClearedMemory(1024*sizeof(define_s*));hash[1023]=&d;assert(hash[0]==nullptr);FreeMemory((char*)hash);
 puts("Native menu token/define copies, alignment and full hash table: ASan/UBSan passed");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+types+functions+tests)
 subprocess.run(['clang++','-I'+str(repo/'ports/ios/engine'),'-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

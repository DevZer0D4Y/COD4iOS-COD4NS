#!/usr/bin/env python3
"""Run production archive reader/writer with truncated and hostile lengths."""
from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parents[4]/'src/script/scr_readwrite.cpp').read_text()
begin=source.index('static constexpr int MAX_SAVE_SOURCE_BUFFERS')
readers=source[begin:source.index('void __cdecl AddSaveStackInternal',begin)]
begin=source.index('void __cdecl Scr_SaveSourceImmediate(')
writer=source[begin:source.index('\n}\n',begin)+3]
stubs=r'''
#include <cassert>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <climits>
#define __cdecl
#define iassert(x) assert(x)
#define ERR_DROP 1
struct MemoryFile {unsigned char flags[2]={1,0};int at=0;};
struct SaveImmediate {void *f=(void*)1;};
struct SaveSourceBufferInfo {char *sourceBuf=nullptr;int len=0;};
struct SourceBufferInfo {bool archive=false;int len=0;char *sourceBuf=nullptr;};
struct {SaveSourceBufferInfo *saveSourceBufferLookup=nullptr;unsigned saveSourceBufferLookupLen=0;}scrParserGlob;
struct {SourceBufferInfo *sourceBufferLookup=nullptr;unsigned sourceBufferLookupLen=0;}scrParserPub;
struct {bool developer=true;unsigned char developer_script=0;}scrVarPub;
std::vector<unsigned char> input,output;size_t position=0;int closes=0;
std::vector<void*> allocations;
void MemFile_ReadData(MemoryFile *m,int n,unsigned char *p){assert(n==1 && m->at<2);*p=m->flags[m->at++];}
int ReadFromDevice(void *p,int n,void *handle){if(!handle)return 0;size_t got=std::min(size_t(n),input.size()-position);if(p)std::memcpy(p,input.data()+position,got);position+=got;return int(got);}
void CloseDevice(void*){++closes;}
int SaveDevice_RemainingBytes(void*){return int(input.size()-position);}
void Com_Error(int,const char*){throw std::runtime_error("corrupt archive");}
void *Hunk_AllocDebugMem(size_t n,const char*){assert(n<=16*1024*1024);void *p=std::malloc(n);assert(p);allocations.push_back(p);return p;}
void SaveMemory_SaveWriteImmediate(const void *p,unsigned n,SaveImmediate *s){if(!s->f)return;const auto *b=(const unsigned char*)p;output.insert(output.end(),b,b+n);}
void add(int n){unsigned char b[4];std::memcpy(b,&n,4);input.insert(input.end(),b,b+4);}
void reset(){for(auto p:allocations)std::free(p);allocations.clear();scrParserGlob={};input.clear();output.clear();position=0;closes=0;}
'''
tests=r'''
int main(){
 for(bool skip:{false,true}){
  auto run=[&](bool bad){MemoryFile m;bool failed=false;try{if(skip)Scr_SkipSource(&m,(void*)1);else Scr_LoadSource(&m,(void*)1);}catch(const std::runtime_error&){failed=true;}assert(failed==bad);if(bad)assert(closes==1 && !scrParserGlob.saveSourceBufferLookup);};
  reset();add(2);add(3);input.insert(input.end(),{'a','b','c'});add(-1);run(false);assert(position==input.size());
  if(!skip){assert(scrParserGlob.saveSourceBufferLookupLen==2);assert(scrParserGlob.saveSourceBufferLookup[1].len==3);assert(!std::memcmp(scrParserGlob.saveSourceBufferLookup[1].sourceBuf,"abc",3));}
  for(int count:{-1,INT_MAX,65537}){reset();add(count);run(true);}
  reset();input={1,0};run(true);
  for(int length:{INT_MAX,-2,16777217}){reset();add(1);add(length);run(true);}
  reset();add(1);add(128);input.push_back('x');run(true);
  reset();add(1);input.push_back(0);run(true);
  reset();add(0);run(false);
 }
 reset();char text[]="abc";SourceBufferInfo info{true,3,text};scrParserPub={&info,1};SaveImmediate save;
 Scr_SaveSourceImmediate(&save);assert(save.f && output.size()==11);
 info.sourceBuf=nullptr;output.clear();save.f=(void*)1;Scr_SaveSourceImmediate(&save);assert(!save.f && output.empty());
 info.sourceBuf=text;info.len=16777217;save.f=(void*)1;Scr_SaveSourceImmediate(&save);assert(!save.f);
 reset();
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-source-bounds-') as directory:
 root=Path(directory);cpp=root/'test.cpp';cpp.write_text(stubs+writer+readers+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
print('Production script archive: valid ordering, truncation, allocation bounds and invalid-write rejection passed.')

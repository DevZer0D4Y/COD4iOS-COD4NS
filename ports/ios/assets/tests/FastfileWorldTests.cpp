#include "FastfileWorld.hpp"
#include <zlib.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>

using Bytes=std::vector<std::uint8_t>;
using namespace kisakcod::assets;
static int checks=0;
static void require(bool condition,const char* message) {++checks;if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
static void put(Bytes& b,std::size_t o,std::uint32_t n) {for(int i=0;i<4;++i)b.at(o+i)=n>>(8*i);}
static void put16(Bytes& b,std::size_t o,std::uint16_t n) {b.at(o)=n;b.at(o+1)=n>>8;}
static void scalar(Bytes& b,std::size_t o,float f) {std::uint32_t n;std::memcpy(&n,&f,4);put(b,o,n);}
static void text(Bytes& b,const char* str) {while(*str)b.push_back(*str++);b.push_back(0);}
static Bytes packed(const Bytes& raw) {
    uLongf len=compressBound(raw.size());Bytes result(12+len);
    std::memcpy(result.data(),"IWffu100",8);put(result,8,5);
    require(compress2(result.data()+12,&len,raw.data(),raw.size(),9)==Z_OK,"compress fixture");
    result.resize(12+len);return result;
}
static Bytes fixture(bool inlineName=false) {
    Bytes b(732);
    put(b,0,inlineName?0xffffffff:0x40001001);put(b,4,0xffffffff);
    put(b,16,6);put(b,20,0xffffffff);put(b,24,1);put(b,48,4);put(b,52,0xffffffff);
    put(b,584,1);put(b,660,0xffffffff);
    if(inlineName)text(b,"maps/mp/mp_test.d3dbsp");
    text(b,"mp_test");
    auto indices=b.size();b.resize(b.size()+12);
    put16(b,indices,99);put16(b,indices+2,99);put16(b,indices+4,99);
    put16(b,indices+6,2);put16(b,indices+8,0);put16(b,indices+10,1);
    auto vertices=b.size();b.resize(b.size()+4*44);
    for(int i=0;i<4;++i) {
        auto o=vertices+i*44;scalar(b,o,float(i));scalar(b,o+4,float(i*2));scalar(b,o+8,32);
        b[o+16]=51;b[o+17]=102;b[o+18]=204;b[o+19]=255;
        b[o+36]=127;b[o+37]=127;b[o+38]=254;b[o+39]=63;
    }
    auto surface=b.size();b.resize(b.size()+48);
    put(b,surface+4,1);put16(b,surface+8,3);put16(b,surface+10,1);put(b,surface+12,3);
    return b;
}
static void reject(const Bytes& bytes,const char* reason) {
    bool failed=false;try{(void)loadFastfileWorld(bytes,"mp_test");}catch(const BspError&){failed=true;}
    require(failed,reason);
}
int main() {
    auto raw=fixture();auto compressed=packed(raw);
    for(bool inlineName:{false,true}) {
        auto mesh=loadFastfileWorld(packed(fixture(inlineName)),"mp_test");
        require(mesh.vertices.size()==4&&mesh.indices==std::vector<std::uint32_t>({3,1,2}),"nonzero base vertex/index offsets");
        require(mesh.minBounds[0]==1&&mesh.maxBounds[0]==3,"bounds exclude unreferenced vertices");
        require(std::abs(mesh.vertices[0].color[0]-.8f)<.001f,"BGRA conversion");
        require(std::abs(mesh.vertices[0].normal[2]-1)<.001f,"packed normal decoding");
    }
    for(std::size_t i=0;i<compressed.size();++i) reject(Bytes(compressed.begin(),compressed.begin()+i),"truncated zlib accepted");
    for(std::size_t i=0;i<raw.size();i+=7) reject(packed(Bytes(raw.begin(),raw.begin()+i)),"truncated world accepted");
    auto changed=compressed;changed[8]=6;reject(changed,"unknown version accepted");
    changed=compressed;changed.push_back(0);reject(changed,"zlib trailing data accepted");
    changed=compressed;changed.back()^=128;reject(changed,"corrupt checksum accepted");
    auto corrupt=raw;put(corrupt,48,1000001);reject(packed(corrupt),"vertex budget exceeded");
    corrupt=raw;put(corrupt,corrupt.size()-48+4,4000);reject(packed(corrupt),"invalid vertex range accepted");
    corrupt=raw;put(corrupt,corrupt.size()-48+12,6);reject(packed(corrupt),"invalid index range accepted");
    corrupt=raw;put16(corrupt,732+8+6,3);reject(packed(corrupt),"invalid relative index accepted");
    corrupt=raw;scalar(corrupt,732+8+12,std::numeric_limits<float>::quiet_NaN());reject(packed(corrupt),"NaN vertex accepted");
    // MapEnts after the world: valid disk32 name reference, inline text and length.
    corrupt=raw;std::string entities="{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"mp_dm_spawn\"\n\"origin\" \"1 2 3\"\n\"angles\" \"0 90 0\"\n}\n";
    auto offset=corrupt.size();corrupt.resize(offset+12);put(corrupt,offset,0x40001001);put(corrupt,offset+4,0xffffffff);put(corrupt,offset+8,entities.size()+1);text(corrupt,entities.c_str());
    auto mesh=loadFastfileWorld(packed(corrupt),"mp_test");
    require(mesh.spawn&&mesh.spawn->position[2]==3&&mesh.spawn->angles[1]==90,"MapEnts spawn decoding");
    bool failed=false;try{loadFastfileWorld(compressed,"renamed");}catch(const BspError&){failed=true;}
    require(failed,"renamed map must report missing world");
    std::cout<<checks<<" fastfile checks passed\n";
}

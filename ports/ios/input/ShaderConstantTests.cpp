#include "../../../src/gfx_d3d/r_shader_constant_sort.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <random>
#include <cstdio>
struct Block { uint32_t count=0; uint16_t dest[16]; const float *value[16]; };
int main() {
    std::array<float,16> values{}; std::array<int,16> order{};
    for (int i=0;i<16;++i) { values[i]=i; order[i]=i; }
    std::mt19937 random(7);
    for (int run=0;run<1000;++run) {
        std::shuffle(order.begin(),order.end(),random); Block block;
        for(int index:order) assert(R_InsertShaderConstant(block,index,&values[index]));
        for(int i=0;i<16;++i) { assert(block.dest[i]==i); assert(block.value[i]==&values[i]); }
        assert(!R_InsertShaderConstant(block,20,&values[0])); assert(block.count==16);
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    float all[]={-1,0,1,nan,std::numeric_limits<float>::infinity()};
    for(float a:all) for(float b:all) for(float c:all) {
        assert(R_CompareShaderFloat(a,a)==0);
        assert(R_CompareShaderFloat(a,b)==-R_CompareShaderFloat(b,a));
        if(R_CompareShaderFloat(a,b)<0 && R_CompareShaderFloat(b,c)<0) assert(R_CompareShaderFloat(a,c)<0);
    }
    puts("Shader constant permutation and strict ordering tests passed");
}

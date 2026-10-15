#include "../../d3d9/metal/ShaderConstantCache.h"
#include <cassert>
#include <array>
#include <memory>
#include <vector>
struct Block {void *pointer=nullptr;};
int main(){
    kisak::metal::ShaderConstantCache<Block> vertex,pixel;
    std::vector<std::unique_ptr<unsigned char[]>> arena;
    auto allocate=[&](size_t n){auto p=std::make_unique<unsigned char[]>(n);Block b{p.get()};arena.push_back(std::move(p));return b;};
    std::array<unsigned char,4096> v{};std::array<unsigned char,3584> p{};
    for(int draw=0;draw<2000;++draw){
        auto a=vertex.Upload(v.data(),v.size(),4096,allocate);
        auto b=pixel.Upload(p.data(),p.size(),3584,allocate);
        assert(a.pointer && b.pointer && !memcmp(a.pointer,v.data(),v.size()) && !memcmp(b.pointer,p.data(),p.size()));
    }
    assert(arena.size()==2); // 4,000 per-draw uploads collapse to two allocations.
    struct Queued {Block block;std::array<unsigned char,4096> expected;};std::vector<Queued> queued;
    for(int draw=0;draw<2500;++draw){
        v[draw%v.size()]=draw%251;
        queued.push_back({vertex.Upload(v.data(),v.size(),4096,allocate),v});
    }
    // Simulate GPU reads delayed until after later CPU constant updates.
    for(const auto &draw:queued)assert(!memcmp(draw.block.pointer,draw.expected.data(),4096));
    auto count=arena.size();vertex.Reset();assert(vertex.Upload(v.data(),v.size(),4096,allocate).pointer && arena.size()==count+1);
    v[1]^=1;auto fail=[](size_t){return Block{};};assert(!vertex.Upload(v.data(),v.size(),4096,fail).pointer);
    assert(vertex.Upload(nullptr,64,4096,allocate).pointer);auto zero=vertex.Upload(nullptr,64,4096,fail);assert(zero.pointer);
    for(int i=0;i<64;++i)assert(static_cast<unsigned char*>(zero.pointer)[i]==0);
    assert(!vertex.Upload(v.data(),4097,4096,allocate).pointer);
}

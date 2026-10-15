#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace kisak::metal {
// Each successful upload is immutable until the frame ends. Reusing identical
// bytes is safe across shaders; a changed block must get a new arena range so
// already-encoded draws keep their original constants.
template<class Allocation> class ShaderConstantCache {
    size_t bytes=0;
    Allocation allocation{};
    bool valid=false;
public:
    void Reset(){valid=false;allocation={};}
    template<class Allocate> Allocation Upload(const void *source,size_t size,size_t limit,Allocate allocate) {
        if(!size || size>limit)return {};
        const auto *previous=static_cast<const uint8_t*>(allocation.pointer);
        if(valid && bytes==size && (source ? !memcmp(source,previous,size)
            : std::all_of(previous,previous+size,[](uint8_t v){return v==0;})))return allocation;
        Allocation next=allocate(size);if(!next.pointer)return {};
        if(source)memcpy(next.pointer,source,size);else memset(next.pointer,0,size);
        bytes=size;allocation=next;valid=true;
        return allocation;
    }
};
}

#pragma once
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>
namespace kisak::save {
// Verify the closed temporary file before replacing a previous checkpoint.
// A failed commit retains both the previous save and the temporary file.
inline bool VerifyAndCommit(const char *temporary,const char *destination,
                            const void *header,size_t headerSize,const void *body,size_t bodySize,
                            uint64_t expectedSize) {
    FILE *f=std::fopen(temporary,"rb");if(!f)return false;
    struct stat st{};
    bool ok=!fstat(fileno(f),&st) && uint64_t(st.st_size)==expectedSize
        && expectedSize>=headerSize+bodySize;
    unsigned char chunk[16384];
    const void *parts[]={header,body};const size_t sizes[]={headerSize,bodySize};
    for(int p=0;ok && p<2;++p) {
        const auto *bytes=static_cast<const unsigned char*>(parts[p]);size_t remaining=sizes[p];
        while(ok && remaining) {
            const size_t n=remaining<sizeof(chunk)?remaining:sizeof(chunk);
            ok=std::fread(chunk,1,n,f)==n && !std::memcmp(chunk,bytes,n);
            remaining-=n;bytes+=n;
        }
    }
    std::fclose(f);
    if(!ok){errno=EIO;return false;}
    return std::rename(temporary,destination)==0;
}
}

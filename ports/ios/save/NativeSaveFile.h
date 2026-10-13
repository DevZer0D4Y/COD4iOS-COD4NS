#pragma once
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "SaveIntegrity.h"
namespace kisak::save {
// Copy to a sibling temporary file; failed copies never truncate the backup.
inline bool PreservePrevious(const char *destination,bool requireValid=false) {
    // Do not rotate a damaged primary over the last intact backup.
    if(requireValid && !savefile::Complete(destination))return true;
    FILE *in=std::fopen(destination,"rb");
    if(!in)return errno==ENOENT;
    const std::string backup=std::string(destination)+".bak", staging=backup+".tmp";
    FILE *out=std::fopen(staging.c_str(),"wb");
    if(!out){std::fclose(in);return false;}
    unsigned char chunk[16384];size_t n;bool ok=true;
    while((n=std::fread(chunk,1,sizeof(chunk),in)))
        if(std::fwrite(chunk,1,n,out)!=n){ok=false;break;}
    ok=ok && !std::ferror(in);
    if(std::fclose(in))ok=false;
    if(std::fflush(out) || fsync(fileno(out)))ok=false;
    if(std::fclose(out))ok=false;
    if(!ok){std::remove(staging.c_str());errno=EIO;return false;}
    return std::rename(staging.c_str(),backup.c_str())==0;
}
// Verify and sync the closed temporary file before replacing a checkpoint.
// Retain one previous generation, including its archived script/demo tail.
inline bool VerifyAndCommit(const char *temporary,const char *destination,
                            const void *header,size_t headerSize,const void *body,size_t bodySize,
                            uint64_t expectedSize) {
    if(!temporary || !destination || !std::strcmp(temporary,destination)
       || (headerSize && !header) || (bodySize && !body)
       || headerSize>expectedSize || bodySize>expectedSize-headerSize){errno=EINVAL;return false;}
    FILE *f=std::fopen(temporary,"rb");if(!f)return false;
    struct stat st{};
    bool ok=!fstat(fileno(f),&st) && st.st_size>=0 && uint64_t(st.st_size)==expectedSize;
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
    if(ok && fsync(fileno(f)))ok=false;
    if(std::fclose(f))ok=false;
    if(!ok){errno=EIO;return false;}
    if(headerSize==savefile::HeaderBytes && !savefile::Complete(temporary)){errno=EIO;return false;}
    if(!PreservePrevious(destination,headerSize==savefile::HeaderBytes))return false;
    return std::rename(temporary,destination)==0;
}
}

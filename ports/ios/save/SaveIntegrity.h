#pragma once
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace kisak::savefile {
constexpr size_t HeaderBytes=1108,BodyLimit=1572864,FileLimit=32*1024*1024;
inline uint32_t Word(const unsigned char *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint32_t CRC(const void *data,size_t length,uint32_t previous=0){
    static const auto table=[](){std::array<uint32_t,256> t{};
        for(uint32_t i=0;i<256;++i){uint32_t c=i;for(int bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320u:0);t[i]=c;}return t;}();
    uint32_t crc=~previous;const auto *bytes=static_cast<const unsigned char*>(data);
    for(size_t i=0;i<length;++i)crc=table[(crc^bytes[i])&255]^(crc>>8);
    return ~crc;
}
inline bool Complete(const char *path){
    if(!path || !*path)return false;
    FILE *f=std::fopen(path,"rb");if(!f)return false;
    std::array<unsigned char,HeaderBytes> header{};struct stat st{};
    bool ok=!fstat(fileno(f),&st) && st.st_size>=int64_t(HeaderBytes) && st.st_size<=int64_t(FileLimit)
        && std::fread(header.data(),1,header.size(),f)==header.size();
    const uint32_t body=Word(header.data()+1104);
    ok=ok && Word(header.data())==287 && body>0 && body<=BodyLimit
        && uint64_t(st.st_size)>=HeaderBytes+body;
    unsigned char block[16384];size_t remaining=body;uint32_t checksum=0;
    while(ok && remaining){size_t n=remaining<sizeof(block)?remaining:sizeof(block);
        ok=std::fread(block,1,n,f)==n;if(ok)checksum=CRC(block,n,checksum);remaining-=n;}
    ok=ok && checksum==Word(header.data()+8);
    std::fclose(f);return ok;
}
// Copy into a sibling temporary file. Never truncate an existing destination
// or backup when a write, sync, validation or rename fails.
inline bool Copy(const char *source,const char *destination){
    if(!Complete(source))return false;
    FILE *in=std::fopen(source,"rb");if(!in)return false;
    std::string stage=std::string(destination)+".tmp";FILE *out=std::fopen(stage.c_str(),"wb");
    if(!out){std::fclose(in);return false;}
    unsigned char block[16384];bool ok=true;size_t n;
    while((n=std::fread(block,1,sizeof(block),in)))if(std::fwrite(block,1,n,out)!=n){ok=false;break;}
    ok=ok && !std::ferror(in);if(std::fclose(in))ok=false;
    if(std::fflush(out) || fsync(fileno(out)))ok=false;
    if(std::fclose(out))ok=false;
    if(!ok || !Complete(stage.c_str())){std::remove(stage.c_str());return false;}
    if(std::rename(stage.c_str(),destination)){std::remove(stage.c_str());return false;}
    return true;
}
inline bool Backup(const char *path){return Copy(path,(std::string(path)+".bak").c_str());}
inline bool Recover(const char *path){
    if(Complete(path))return true;
    const std::string backup=std::string(path)+".bak",corrupt=std::string(path)+".corrupt";
    if(!Complete(backup.c_str()))return false;
    struct stat st{};const bool existed=!lstat(path,&st);
    if(existed && (!S_ISREG(st.st_mode) || std::rename(path,corrupt.c_str())))return false;
    if(Copy(backup.c_str(),path))return true;
    if(existed)std::rename(corrupt.c_str(),path);
    return false;
}
}

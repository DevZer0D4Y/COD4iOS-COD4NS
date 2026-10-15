#pragma once
#include "SaveResume.h"
#include <algorithm>

namespace kisak::campaign {
struct Progress {
    unsigned unlocked=0;
    std::string difficulty=std::string(50,'0');
};
inline bool Valid(const Progress &p) {
    return p.unlocked<=50 && p.difficulty.size()==50
        && p.difficulty.find_first_not_of("01234")==std::string::npos;
}
inline Progress Merge(Progress a,const Progress &b) {
    if(!Valid(a))a={};if(!Valid(b))return a;
    a.unlocked=std::max(a.unlocked,b.unlocked);
    for(size_t i=0;i<50;++i)a.difficulty[i]=std::max(a.difficulty[i],b.difficulty[i]);
    return a;
}
inline std::string Path(const std::string &players,const std::string &profile) {
    return players+"/profiles/"+profile+"/.cod4-campaign";
}
inline Progress Load(const std::string &players,const std::string &profile) {
    if(!savefile::ResumeComponent(profile))return {};
    FILE *in=fopen(Path(players,profile).c_str(),"rb");if(!in)return {};
    std::array<unsigned char,64> bytes{};
    bool ok=fread(bytes.data(),1,64,in)==64 && fgetc(in)==EOF && !ferror(in);fclose(in);
    if(!ok || savefile::Word(bytes.data())!=0x31475043u
        || savefile::Word(bytes.data()+60)!=savefile::CRC(bytes.data(),60))return {};
    Progress result{savefile::Word(bytes.data()+4),std::string(reinterpret_cast<char*>(bytes.data()+8),50)};
    return Valid(result)?result:Progress{};
}
inline bool Save(const std::string &players,const std::string &profile,const Progress &progress) {
    if(!savefile::ResumeComponent(profile) || !Valid(progress))return false;
    const auto value=Merge(progress,Load(players,profile));
    std::array<unsigned char,64> bytes{};
    const uint32_t magic=0x31475043u;
    memcpy(bytes.data(),&magic,4);memcpy(bytes.data()+4,&value.unlocked,4);
    memcpy(bytes.data()+8,value.difficulty.data(),50);
    const auto crc=savefile::CRC(bytes.data(),60);memcpy(bytes.data()+60,&crc,4);
    const std::string destination=Path(players,profile),pattern=destination+".pending.XXXXXX";
    std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
    int fd=mkstemp(temporary.data());if(fd<0)return false;
    FILE *out=fdopen(fd,"wb");if(!out){close(fd);unlink(temporary.data());return false;}
    bool ok=fwrite(bytes.data(),1,64,out)==64;
    if(fflush(out) || fsync(fd))ok=false;
    if(fclose(out))ok=false;
    if(ok)ok=rename(temporary.data(),destination.c_str())==0;
    if(!ok)unlink(temporary.data());return ok;
}
inline unsigned ReachedFromSaves(const std::string &players,const std::string &profile) {
    if(!savefile::ResumeComponent(profile))return 0;
    const std::string directory=players+"/profiles/"+profile+"/save/autosave";
    DIR *dir=opendir(directory.c_str());if(!dir)return 0;
    const char *maps[]={"killhouse","cargoship","coup","blackout","armada","bog_a","hunted","ac130","bog_b","airlift","aftermath","village_assault","scoutsniper","sniperescape","village_defend","ambush","icbm","launchfacility_a","launchfacility_b","jeepride","airplane"};
    unsigned reached=0;
    while(auto *entry=readdir(dir)){
        const std::string name=entry->d_name,path=directory+"/"+name;
        if(name.size()<4 || name.compare(name.size()-4,4,".svg") || !savefile::Complete(path.c_str()))continue;
        FILE *in=fopen(path.c_str(),"rb");if(!in)continue;
        char map[257]{};bool ok=!fseek(in,32,SEEK_SET) && fread(map,1,256,in)==256;fclose(in);
        if(!ok || !memchr(map,0,256))continue;
        for(unsigned i=0;i<21;++i)if(!strcmp(map,maps[i])){
            unsigned unlocked=i;
            if(name.find("-levelend.svg")!=std::string::npos)unlocked=std::min(i+1,20u);
            reached=std::max(reached,unlocked);break;
        }
    }
    closedir(dir);return reached;
}
}

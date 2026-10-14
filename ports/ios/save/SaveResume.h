#pragma once
#include "SaveIntegrity.h"
#include <dirent.h>
#include <vector>
#include <cstdlib>

namespace kisak::savefile {
inline bool ResumeNewer(const struct stat &a,const struct stat &b) {
#if defined(__APPLE__)
    const auto x=a.st_mtimespec,y=b.st_mtimespec;
#else
    const auto x=a.st_mtim,y=b.st_mtim;
#endif
    return x.tv_sec>y.tv_sec || (x.tv_sec==y.tv_sec && x.tv_nsec>y.tv_nsec);
}
inline bool ResumeComponent(const std::string &name) {
    if(name.empty() || name=="." || name=="..")return false;
    for(unsigned char c:name)if(c<32 || c==127 || c=='/' || c=='\\')return false;
    return true;
}
inline bool ResumePath(const std::string &profile,const std::string &relative) {
    const std::string prefix="profiles/"+profile+"/save/";
    if(!ResumeComponent(profile) || relative.size()>=64 || relative.compare(0,prefix.size(),prefix))return false;
    if(relative.size()<4 || relative.compare(relative.size()-4,4,".svg"))return false;
    size_t start=prefix.size();
    while(start<relative.size()){
        size_t end=relative.find('/',start);
        if(end==std::string::npos)end=relative.size();
        if(!ResumeComponent(relative.substr(start,end-start)))return false;
        start=end+1;
    }
    return true;
}
inline bool ResumeComplete(const std::string &players,const std::string &profile,const std::string &relative) {
    if(!ResumePath(profile,relative))return false;
    const std::string physical=players+"/"+relative;
    struct stat st{};
    if(!lstat(physical.c_str(),&st) && !S_ISREG(st.st_mode))return false;
    return Complete(physical.c_str()) || Recover(physical.c_str());
}
// Record the resume pointer independently of config.cfg, which the engine
// rewrites in place. A crash must not hide a durable checkpoint from the menu.
inline bool RecordResume(const std::string &players,const std::string &profile,const std::string &relative) {
    if(!ResumeComplete(players,profile,relative))return false;
    const std::string marker=players+"/profiles/"+profile+"/.cod4-resume";
    std::string pattern=marker+".pending.XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    const int fd=mkstemp(name.data());if(fd<0)return false;
    FILE *out=fdopen(fd,"wb");
    if(!out){close(fd);unlink(name.data());return false;}
    bool ok=fwrite(relative.data(),1,relative.size(),out)==relative.size();
    if(fflush(out) || fsync(fd))ok=false;
    if(fclose(out))ok=false;
    if(ok)ok=rename(name.data(),marker.c_str())==0;
    if(!ok)unlink(name.data());
    return ok;
}
inline std::string ResolveResume(const std::string &players,const std::string &profile,const std::string &current) {
    if(!ResumeComponent(profile))return {};
    const bool currentValid=ResumeComplete(players,profile,current);
    const std::string directory=players+"/profiles/"+profile;
    std::string marked;
    if(FILE *in=fopen((directory+"/.cod4-resume").c_str(),"rb")){
        char buffer[65]{};const size_t count=fread(buffer,1,sizeof(buffer),in);fclose(in);
        if(count<64)marked.assign(buffer,count);
    }
    if(ResumeComplete(players,profile,marked)){
        struct stat a{},b{};
        if(!currentValid || (!stat((players+"/"+marked).c_str(),&a)
            && !stat((players+"/"+current).c_str(),&b) && ResumeNewer(a,b)))return marked;
    }
    if(currentValid)return current;
    const std::string prefix="profiles/"+profile+"/save/autosave/";
    DIR *dir=opendir((players+"/"+prefix).c_str());if(!dir)return {};
    std::string latest;struct stat newest{};
    while(auto *entry=readdir(dir)){
        const std::string candidate=prefix+entry->d_name;
        if(!ResumeComplete(players,profile,candidate))continue;
        struct stat st{};
        if(!stat((players+"/"+candidate).c_str(),&st) && (latest.empty() || ResumeNewer(st,newest))){
            latest=candidate;newest=st;
        }
    }
    closedir(dir);return latest;
}
}

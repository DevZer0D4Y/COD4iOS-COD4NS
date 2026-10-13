#pragma once
#include "SaveIntegrity.h"
#include <dirent.h>

namespace kisak::savefile {
inline void PrepareDirectory(const std::string &directory,unsigned depth=0){
    if(depth>8)return;
    DIR *dir=opendir(directory.c_str());if(!dir)return;
    while(auto *entry=readdir(dir)){
        if(entry->d_name[0]=='.')continue;
        const std::string path=directory+"/"+entry->d_name;struct stat st{};
        if(lstat(path.c_str(),&st))continue;
        if(S_ISDIR(st.st_mode)){PrepareDirectory(path,depth+1);continue;}
        if(!S_ISREG(st.st_mode) || path.find("/save/")==std::string::npos)continue;
        if(path.size()>4 && path.compare(path.size()-4,4,".svg")==0){
            if(!Complete(path.c_str()))Recover(path.c_str());
            if(Complete(path.c_str()) && access((path+".bak").c_str(),F_OK)!=0)Backup(path.c_str());
        }else if(path.size()>8 && path.compare(path.size()-8,8,".svg.bak")==0){
            const std::string target=path.substr(0,path.size()-4);
            if(access(target.c_str(),F_OK)!=0)Recover(target.c_str());
        }
    }
    closedir(dir);
}
}

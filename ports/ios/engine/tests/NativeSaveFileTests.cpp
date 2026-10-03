#include "../../save/NativeSaveFile.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <string>
#include <unistd.h>
int main() {
    char dir[]="/tmp/cod4ios-native-save-XXXXXX";assert(mkdtemp(dir));
    const std::string temp=std::string(dir)+"/temp.svg",dest=std::string(dir)+"/checkpoint.svg";
    auto write=[](const std::string &p,const std::string &s){std::ofstream(p,std::ios::binary)<<s;};
    auto read=[](const std::string &p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
    const std::string header="HEADER",body(50000,'x'),tail="SCRIPT SOURCE";write(dest,"old checkpoint");
    write(temp,header+body+tail);
    assert(!kisak::save::VerifyAndCommit(temp.c_str(),dest.c_str(),header.data(),header.size(),body.data(),body.size(),header.size()+body.size()));
    assert(read(dest)=="old checkpoint" && std::filesystem::exists(temp));
    assert(!kisak::save::VerifyAndCommit(temp.c_str(),(std::string(dir)+"/missing/checkpoint.svg").c_str(),header.data(),header.size(),body.data(),body.size(),header.size()+body.size()+tail.size()));
    assert(read(dest)=="old checkpoint" && std::filesystem::exists(temp));
    write(temp,header+std::string(50000,'y')+tail);
    assert(!kisak::save::VerifyAndCommit(temp.c_str(),dest.c_str(),header.data(),header.size(),body.data(),body.size(),header.size()+body.size()+tail.size()));
    assert(read(dest)=="old checkpoint");write(temp,header+body+tail);
    assert(kisak::save::VerifyAndCommit(temp.c_str(),dest.c_str(),header.data(),header.size(),body.data(),body.size(),header.size()+body.size()+tail.size()));
    assert(read(dest)==header+body+tail && !std::filesystem::exists(temp));std::filesystem::remove_all(dir);
}

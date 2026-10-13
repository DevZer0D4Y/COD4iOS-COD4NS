#include "../../save/NativeSaveFile.h"
#include "../../save/SaveRecovery.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <string>
static std::string fixture(char value){
    std::string data(1108+512+32,0);int version=287,body=512;
    std::memcpy(data.data(),&version,4);std::memcpy(data.data()+1104,&body,4);
    std::memset(data.data()+1108,value,body);auto crc=kisak::savefile::CRC(data.data()+1108,body);
    std::memcpy(data.data()+8,&crc,4);return data;
}
static void write(const std::string &path,const std::string &data){std::ofstream(path,std::ios::binary).write(data.data(),data.size());}
static std::string read(const std::string &path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(){
    using namespace kisak::savefile;
    assert(CRC("123456789",9)==0xcbf43926u);
    assert(CRC("56789",5,CRC("1234",4))==CRC("123456789",9));
    char folder[]="/tmp/cod4-integrity-XXXXXX";assert(mkdtemp(folder));const std::string root=folder;
    const char *maps[]={"killhouse","cargoship","coup","blackout","armada","bog_a","hunted","ac130","bog_b","airlift","aftermath","village_assault","scoutsniper","sniperescape","village_defend","ambush","icbm","launchfacility_a","launchfacility_b","jeepride","airplane"};
    for(const auto *map:maps){
        const std::string path=root+"/players/profiles/Tester/save/autosave/"+map+".svg",temporary=root+"/temp.svg";
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());auto old=fixture('a'),next=fixture('b');write(path,old);
        write(temporary,next);
        assert(kisak::save::VerifyAndCommit(temporary.c_str(),path.c_str(),next.data(),1108,next.data()+1108,512,next.size()));
        assert(read(path)==next && read(path+".bak")==old);
        auto bad=next;bad[1120]^=1;write(path,bad);assert(!Complete(path.c_str()));
        PrepareDirectory(root+"/players");assert(read(path)==old && read(path+".corrupt")==bad);
        // A bad replacement cannot destroy the primary or its healthy backup.
        write(temporary,bad);
        assert(!kisak::save::VerifyAndCommit(temporary.c_str(),path.c_str(),bad.data(),1108,bad.data()+1108,512,bad.size()));
        assert(read(path)==old && read(path+".bak")==old);
        std::filesystem::remove(path);assert(Recover(path.c_str()));assert(read(path)==old);
    }
    std::filesystem::remove_all(root);
}

#include "../../save/CampaignProgress.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
static void saveFixture(const std::string &path,const char *map,bool valid=true){
    std::string data(1108+256,0);int version=287,body=256;
    memcpy(data.data(),&version,4);memcpy(data.data()+1104,&body,4);strcpy(data.data()+32,map);
    auto crc=kisak::savefile::CRC(data.data()+1108,body);memcpy(data.data()+8,&crc,4);
    if(!valid)data[1110]^=1;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream(path,std::ios::binary).write(data.data(),data.size());
}
int main(){
    using namespace kisak::campaign;
    char directory[]="/tmp/cod4-campaign-XXXXXX";assert(mkdtemp(directory));const std::string players=std::string(directory)+"/players";
    std::filesystem::create_directories(players+"/profiles/Tester");
    pid_t child=fork();assert(child>=0);
    if(!child){Progress p;p.unlocked=8;p.difficulty[7]='3';assert(Save(players,"Tester",p));_exit(0);}
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    auto p=Load(players,"Tester");assert(p.unlocked==8 && p.difficulty[7]=='3');assert(Load(players,"Other").unlocked==0);
    Progress lower;lower.unlocked=2;lower.difficulty[7]='1';assert(Save(players,"Tester",lower));assert(Load(players,"Tester").unlocked==8 && Load(players,"Tester").difficulty[7]=='3');
    Progress bad;bad.unlocked=51;assert(!Save(players,"Tester",bad));bad={};bad.difficulty="broken";assert(!Save(players,"Tester",bad));assert(Load(players,"Tester").unlocked==8);
    auto blocked=players+"/profiles/Blocked";std::filesystem::create_directories(blocked);std::filesystem::create_directory(Path(players,"Blocked"));assert(!Save(players,"Blocked",p));
    const std::string autosaves=players+"/profiles/Tester/save/autosave/";
    saveFixture(autosaves+"ac130-levelend.svg","ac130");assert(ReachedFromSaves(players,"Tester")==8);
    saveFixture(autosaves+"bog_b.svg","bog_b");assert(ReachedFromSaves(players,"Tester")==8);
    saveFixture(autosaves+"unknown.svg","unrecognized");saveFixture(autosaves+"airplane.svg","airplane",false);assert(ReachedFromSaves(players,"Tester")==8);
    assert(ReachedFromSaves(players,"Other")==0);
    auto marker=Path(players,"Tester");{std::fstream f(marker,std::ios::binary|std::ios::in|std::ios::out);f.seekp(15);f.put('9');}
    assert(Load(players,"Tester").unlocked==0);assert(ReachedFromSaves(players,"Tester")==8);
    std::filesystem::remove_all(directory);
}

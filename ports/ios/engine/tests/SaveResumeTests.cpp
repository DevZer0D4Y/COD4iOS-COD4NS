#include "../../save/SaveResume.h"
#include "../../save/SaveRecovery.h"
#include <filesystem>
#include <fstream>
#include <cassert>
static std::string fixture(char value){
    std::string bytes(1108+256,0);int version=287,body=256;
    memcpy(bytes.data(),&version,4);memcpy(bytes.data()+1104,&body,4);
    memset(bytes.data()+1108,value,body);auto crc=kisak::savefile::CRC(bytes.data()+1108,body);
    memcpy(bytes.data()+8,&crc,4);return bytes;
}
static void write(const std::string &path,const std::string &bytes){
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream(path,std::ios::binary).write(bytes.data(),bytes.size());
}
int main(){
    using namespace kisak::savefile;
    char directory[]="/tmp/cod4-resume-XXXXXX";assert(mkdtemp(directory));
    const std::string players=std::string(directory)+"/players",profile="Tester";
    const std::string checkpoint="profiles/Tester/save/autosave/blackout-inside.svg";
    write(players+"/"+checkpoint,fixture('a'));
    assert(RecordResume(players,profile,checkpoint));
    // No config.cfg or sv_lastSaveGame after a fresh process start.
    assert(ResolveResume(players,profile,"")==checkpoint);
    assert(ResolveResume(players,"Other","").empty());
    assert(!RecordResume(players,profile,"profiles/Other/save/autosave/blackout.svg"));
    assert(!RecordResume(players,profile,"profiles/Tester/save/../config.cfg.svg"));
    assert(!RecordResume(players,profile,"profiles/Tester/save/missing.svg"));
    assert(ResolveResume(players,profile,"profiles/Tester/save/missing.svg")==checkpoint);
    assert(Backup((players+"/"+checkpoint).c_str()));
    std::filesystem::remove(players+"/"+checkpoint);
    assert(ResolveResume(players,profile,checkpoint)==checkpoint);
    assert(Complete((players+"/"+checkpoint).c_str()));
    // Truncated marker/config: fallback stays inside active-profile autosaves.
    write(players+"/profiles/Tester/.cod4-resume",std::string(90,'x'));
    assert(ResolveResume(players,profile,"")==checkpoint);
    const std::string manual="profiles/Tester/save/manual.svg";
    write(players+"/"+manual,fixture('b'));
    assert(ResolveResume(players,profile,manual)==manual);
    std::filesystem::remove(players+"/profiles/Tester/.cod4-resume");
    std::filesystem::create_directory(players+"/profiles/Tester/.cod4-resume");
    assert(!RecordResume(players,profile,checkpoint));
    assert(Complete((players+"/"+checkpoint).c_str()));
    std::filesystem::remove_all(directory);
}

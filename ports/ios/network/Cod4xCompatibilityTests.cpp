#include "cod4x_commands.h"
#include "cod4x_paths.h"
#include <cassert>
#include <cstdio>
int main() {
    cod4x::CommandHistory<> commands;
    for (int i=1;i<=500;++i) { auto text=std::to_string(i); commands.store(i,text.c_str()); }
    assert(std::string(commands.get(1))=="1");
    assert(std::string(commands.get(500))=="500");
    assert(!commands.get(0) && !commands.get(-1) && !commands.get(501));
    commands.store(2049,"wrapped"); assert(!commands.get(1));
    assert(std::string(commands.get(2049))=="wrapped");
    commands.clear(); assert(!commands.get(2049) && !commands.get(500));
    const std::string longCommand = "d 20 " + std::string(7000, 'x');
    commands.store(10, longCommand.c_str());
    assert(std::string(commands.get(10)) == longCommand);
    commands.store(11, std::string(9000, 'y').c_str());
    assert(std::strlen(commands.get(11)) == 8191);
    commands.clear(); assert(!commands.get(10) && !commands.get(11));
    assert(cod4x::UserMapIwd("usermaps/mp_farmhouse/mp_farmhouse"));
    assert(!cod4x::UserMapIwd("usermaps/../main/iw_00"));
    assert(!cod4x::UserMapIwd("main/iw_00"));
    assert(!cod4x::UserMapIwd("usermaps/mp_a/evil.cfg"));
    assert(cod4x::UserMapFastfile("usermaps/mp_farmhouse")=="usermaps/mp_farmhouse/mp_farmhouse.ff");
    assert(cod4x::UserMapFastfile("usermaps/mp_farmhouse_load")=="usermaps/mp_farmhouse/mp_farmhouse_load.ff");
    assert(cod4x::UserMapFastfile("mp_farmhouse")=="usermaps/mp_farmhouse/mp_farmhouse.ff");
    assert(cod4x::UserMapFastfile("usermaps/../main/config").empty());
    assert(cod4x::UserMapFastfile("mp_a/../../config").empty());
    puts("CoD4x command retention and usermap path tests passed");
}

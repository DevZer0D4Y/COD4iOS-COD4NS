#pragma once
#include <string>
namespace cod4x {
inline bool AssetName(const std::string &name) {
    if (name.empty() || name.size() > 63) return false;
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
// Server fastfile references omit the duplicated map-directory component.
inline std::string UserMapFastfile(const std::string &reference) {
    std::string name = reference;
    if (name.compare(0,9,"usermaps/") == 0) name.erase(0,9);
    if (!AssetName(name) || name.compare(0,3,"mp_") != 0) return {};
    std::string map = name;
    if (map.size() > 5 && map.compare(map.size()-5,5,"_load") == 0) map.resize(map.size()-5);
    return "usermaps/" + map + "/" + name + ".ff";
}
inline bool UserMapIwd(const std::string &name) {
    if (name.compare(0,9,"usermaps/") != 0) return false;
    const auto slash = name.find('/',9);
    return slash != std::string::npos && AssetName(name.substr(9,slash-9)) && AssetName(name.substr(slash+1));
}
}

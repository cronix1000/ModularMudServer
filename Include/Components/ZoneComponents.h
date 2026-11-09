#pragma once
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using EntityID = int;

struct ZoneIdentityComponent {
    std::string regionId;
    int zoneId = 0;
    std::string name;
    std::string description;
    json rules;
    std::string zoneScriptRef;
    bool isInstanceSource = false;
};
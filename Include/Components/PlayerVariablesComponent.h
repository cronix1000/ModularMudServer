#pragma once
#include <map>
#include <string>
struct PlayerVariablesComponent {
    std::map<std::string, int> intVars;
    std::map<std::string, std::string> stringVars;
};
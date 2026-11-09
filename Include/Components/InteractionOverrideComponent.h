#pragma once
#include <string>
#include <unordered_map>

struct InteractionOverrideComponent {
    std::unordered_map<std::string, std::vector<std::string>> overrides;
};
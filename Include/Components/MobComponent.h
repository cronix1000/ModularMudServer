#pragma once
#include <string>

struct MobComponent {
    std::string templateId; // Links back to your Excel/JSON data
    float respawnTimer;
    bool isPersistent;
};
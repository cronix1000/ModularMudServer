#pragma once
#include <vector>
#include <string>
struct VoiceComponent {
    std::vector<std::string> idleLines;
    std::vector<std::string> combatLines;
    std::vector<std::string> deathLines;
};
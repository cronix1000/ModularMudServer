#pragma once
#include <string>

struct ArmourComponent {
    std::string templateID;
    int defense = 0;
    int magicDefense = 0;
    std::string type = "cloth";
};
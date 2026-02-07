#pragma once
#include <string>
struct ItemComponent {
    std::string templateName;
    int weight = 0;
    int value = 0;
    bool is_gettable = true; 
    bool is_equippable = false;

};
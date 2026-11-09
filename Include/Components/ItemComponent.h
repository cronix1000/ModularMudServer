#pragma once
#include <string>
#include <vector>
#include "EquipmentSlot.h"
struct ItemComponent {
    std::string templateName;
    int weight = 0;
    int value = 0;
    bool is_gettable = true;
    bool is_equippable = false;
    int primarySkillId = -1;
    std::vector<EquipmentSlot> allowedSlots;
    std::vector<int> extraSkillIds;
};
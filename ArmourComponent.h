#pragma once
#include "EquipmentSlot.h"

struct ArmourComponent {
    std::string templateID;
        int defense = 0;       
        int magicDefense = 0;  // Resistance to spells
        EquipmentSlot slot = EquipmentSlot::Default; 
        std::string type = "cloth"; // "plate", "leather", "cloth"
};
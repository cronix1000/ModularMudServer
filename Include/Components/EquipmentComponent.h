#pragma once
#include <unordered_map>
#include <string>
#include "EquipmentSlot.h"

struct EquipmentComponent {
	std::unordered_map<EquipmentSlot, int> slots;
};
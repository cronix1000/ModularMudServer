#pragma once
#include <map>
#include <string>
#include "EquipmentSlot.h"

struct EquipmentComponent {
	std::map<EquipmentSlot, int> slots;
};
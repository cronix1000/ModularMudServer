#pragma once
#include "EquipmentSlot.h"

struct EquipItemIntentComponent {
	int itemId;
	EquipmentSlot slot = EquipmentSlot::Default;
};

struct UnequipItemIntentComponent {
	int itemId;
	EquipmentSlot slot = EquipmentSlot::Default;
};
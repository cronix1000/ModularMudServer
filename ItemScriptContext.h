#pragma once
#include "EquipmentSlot.h"
#include <string>

struct ItemScriptContext {
	int userID = -1;
	int itemID = -1;
	EquipmentSlot slot = EquipmentSlot::Default;
	std::string itemTemplateID;
	int roomID = -1;
	int x = 0;
	int y = 0;
};

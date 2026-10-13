#pragma once
#include "EquipmentSlot.h"
#include "EquipmentComponent.h"
#include <string>
#include <vector>

namespace EquipmentSlotInfo {

struct SlotInfo {
	EquipmentSlot slot;
	const char* canonical;
	const char* display;
	int multiGroup;
};

const SlotInfo& GetSlotInfo(EquipmentSlot s);
std::vector<EquipmentSlot> SlotsInGroup(int group);
EquipmentSlot ParseSlotString(const std::string& s);
const char* SlotToString(EquipmentSlot s);
EquipmentSlot PickLowestEmptyFinger(const EquipmentComponent& equip);
EquipmentSlot PickFirstAllowedEmpty(const EquipmentComponent& equip, const std::vector<EquipmentSlot>& allowed);
EquipmentSlot FindSlotOfEntity(EquipmentComponent equipment, int itemID);

}
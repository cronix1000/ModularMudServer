#include "EquipmentSlotInfo.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace EquipmentSlotInfo {

namespace {
	const SlotInfo kRegistry[] = {
		{ EquipmentSlot::Helmet,    "helmet",     "Head",       0 },
		{ EquipmentSlot::Chest,     "chest",      "Chest",      0 },
		{ EquipmentSlot::Bracers,   "bracers",    "Bracers",    0 },
		{ EquipmentSlot::Leggings,  "leggings",   "Leggings",   0 },
		{ EquipmentSlot::MainArm,   "main_hand",  "Main Hand",  0 },
		{ EquipmentSlot::OffHand,   "off_hand",   "Off Hand",   0 },
		{ EquipmentSlot::Shoulders, "shoulders",  "Shoulders",  0 },
		{ EquipmentSlot::Knees,     "knees",      "Knees",      0 },
		{ EquipmentSlot::Shoes,     "shoes",      "Boots",      0 },
		{ EquipmentSlot::Necklace,  "necklace",   "Necklace",   0 },
		{ EquipmentSlot::Back,      "back",       "Back",       0 },
		{ EquipmentSlot::Waist,     "waist",      "Waist",      0 },
		{ EquipmentSlot::Wrists,    "wrists",     "Wrists",     0 },
		{ EquipmentSlot::Finger1,   "ring1",      "Ring 1",     1 },
		{ EquipmentSlot::Finger2,   "ring2",      "Ring 2",     1 },
		{ EquipmentSlot::Finger3,   "ring3",      "Ring 3",     1 },
		{ EquipmentSlot::Finger4,   "ring4",      "Ring 4",     1 },
		{ EquipmentSlot::Finger5,   "ring5",      "Ring 5",     1 },
		{ EquipmentSlot::Finger6,   "ring6",      "Ring 6",     1 },
		{ EquipmentSlot::Finger7,   "ring7",      "Ring 7",     1 },
		{ EquipmentSlot::Finger8,   "ring8",      "Ring 8",     1 },
		{ EquipmentSlot::Finger9,   "ring9",      "Ring 9",     1 },
		{ EquipmentSlot::Finger10,  "ring10",     "Ring 10",    1 },
	};

	static_assert(static_cast<int>(EquipmentSlot::Helmet)   == 0,  "slot ordering broken");
	static_assert(static_cast<int>(EquipmentSlot::Finger10) == 22, "slot ordering broken");
	static_assert(static_cast<int>(EquipmentSlot::Default)  == 23, "slot ordering broken");

	void ToLower(std::string& s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	}

	bool SlotsAreContiguous() {
		for (size_t i = 0; i < sizeof(kRegistry) / sizeof(kRegistry[0]); ++i) {
			if (static_cast<int>(kRegistry[i].slot) != static_cast<int>(i)) return false;
		}
		return true;
	}

	static const bool kContiguityOK = SlotsAreContiguous();
}

const SlotInfo& GetSlotInfo(EquipmentSlot s) {
	const int idx = static_cast<int>(s);
	if (idx >= 0 && idx < static_cast<int>(sizeof(kRegistry) / sizeof(kRegistry[0]))) {
		return kRegistry[idx];
	}
	return kRegistry[0];
}

std::vector<EquipmentSlot> SlotsInGroup(int group) {
	std::vector<EquipmentSlot> result;
	for (const auto& info : kRegistry) {
		if (info.multiGroup == group) {
			result.push_back(info.slot);
		}
	}
	return result;
}

EquipmentSlot ParseSlotString(const std::string& input) {
	std::string s = input;
	ToLower(s);
	s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());

	for (const auto& info : kRegistry) {
		if (s == info.canonical) return info.slot;
	}

	if (s == "main")  return EquipmentSlot::MainArm;
	if (s == "off")   return EquipmentSlot::OffHand;
	if (s == "boots") return EquipmentSlot::Shoes;
	if (s == "amulet") return EquipmentSlot::Necklace;
	if (s == "neck")  return EquipmentSlot::Necklace;
	if (s == "cloak") return EquipmentSlot::Back;
	if (s == "cape")  return EquipmentSlot::Back;
	if (s == "belt")  return EquipmentSlot::Waist;
	if (s == "vambraces") return EquipmentSlot::Wrists;
	if (s == "pauldrons") return EquipmentSlot::Shoulders;
	if (s == "legs")  return EquipmentSlot::Leggings;
	if (s == "torso") return EquipmentSlot::Chest;
	if (s == "head")  return EquipmentSlot::Helmet;
	if (s == "hands") return EquipmentSlot::Bracers;
	if (s == "feet")  return EquipmentSlot::Shoes;

	if (s == "ring") {
		auto slots = SlotsInGroup(1);
		return slots.empty() ? EquipmentSlot::Default : slots.front();
	}

	if (s.rfind("finger", 0) == 0 && s.size() > 6) {
		std::string num = s.substr(6);
		int idx = 0;
		try { idx = std::stoi(num); } catch (...) { return EquipmentSlot::Default; }
		if (idx >= 1 && idx <= 10) {
			auto slots = SlotsInGroup(1);
			if (idx - 1 < static_cast<int>(slots.size())) return slots[idx - 1];
		}
	}

	return EquipmentSlot::Default;
}

const char* SlotToString(EquipmentSlot s) {
	return GetSlotInfo(s).display;
}

EquipmentSlot PickLowestEmptyFinger(const EquipmentComponent& equip) {
	for (const auto& info : kRegistry) {
		if (info.multiGroup != 1) continue;
		if (!equip.slots.count(info.slot)) return info.slot;
	}
	return EquipmentSlot::Default;
}

EquipmentSlot PickFirstAllowedEmpty(const EquipmentComponent& equip, const std::vector<EquipmentSlot>& allowed) {
	for (EquipmentSlot slot : allowed) {
		if (slot == EquipmentSlot::Default) continue;
		if (!equip.slots.count(slot)) return slot;
	}
	return EquipmentSlot::Default;
}

EquipmentSlot FindSlotOfEntity(EquipmentComponent equipment, int itemID)
{
	for (auto slot : equipment.slots) {
		if (slot.second == itemID)
			return slot.first;
	}
	return EquipmentSlot::Default;
}
}
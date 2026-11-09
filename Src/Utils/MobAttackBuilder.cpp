#include "MobAttackBuilder.h"
#include "Registry.h"
#include "Component.h"
#include "MobAttackComponent.h"
#include <ctime>
#include <random>

namespace {
	std::mt19937& Rng() {
		static std::mt19937 rng(static_cast<unsigned>(time(nullptr)));
		return rng;
	}
}

namespace MobAttackBuilder {

MobAttackRoll BuildAttack(Registry* registry, int mobID, int targetID) {
	MobAttackRoll roll;
	auto* mobAttack = registry->GetComponent<MobAttackComponent>(mobID);
	if (!mobAttack) return roll;

	if (mobAttack->hasLuaHook && mobAttack->luaAttackFunc.valid()) {
		auto result = mobAttack->luaAttackFunc(mobID, targetID);
		if (result.valid()) {
			sol::table attackData = result;
			roll.valid = true;
			roll.verb = attackData.get_or<std::string>("verb", "hits");
			float defaultMultiplier = 1.0f;
			float multiplier = attackData.get_or("multiplier", defaultMultiplier);
			if (multiplier == defaultMultiplier) {
				multiplier = attackData.get_or("damage", defaultMultiplier);
			}
			roll.multiplier = multiplier;
			roll.damageType = attackData.get_or<std::string>("damage_type", "physical");
		}
		return roll;
	}

	if (!mobAttack->attackPatterns.empty()) {
		std::uniform_int_distribution<size_t> dist(0, mobAttack->attackPatterns.size() - 1);
		const auto& pattern = mobAttack->attackPatterns[dist(Rng())];
		roll.valid = true;
		roll.multiplier = pattern.multiplier;
		roll.verb = pattern.verb;
		roll.damageType = pattern.damageType;
	}

	return roll;
}

}

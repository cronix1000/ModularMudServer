#include "PowerCalculator.h"
#include "Registry.h"
#include "Component.h"
#include "MobAttackComponent.h"
#include <algorithm>
#include <string>

namespace {
	constexpr float STAT_BASE = 5.0f;
	constexpr float STRENGTH_WEIGHT = 2.0f;
	constexpr float DEXTERITY_WEIGHT = 0.5f;
	constexpr float INTELLIGENCE_WEIGHT = 2.0f;
	constexpr float WISDOM_WEIGHT = 0.5f;
	constexpr float MASTERY_BONUS_PER_LEVEL = 0.005f;
	constexpr float MASTERY_BONUS_CAP = 0.5f;
	constexpr float DEFAULT_ATTACK_INTERVAL = 1.0f;
}

namespace PowerCalculator {

void RecalculateDerivedStats(Registry* registry, int entityID) {
	auto* stats = registry->GetComponent<StatComponent>(entityID);
	if (!stats) return;

	float statBase = STAT_BASE + STRENGTH_WEIGHT * stats->Strength + DEXTERITY_WEIGHT * stats->Dexterity;

	float weaponPower = 0.0f;
	int masteryLevel = 1;
	auto* equipment = registry->GetComponent<EquipmentComponent>(entityID);
	if (equipment) {
		auto slotIt = equipment->slots.find(EquipmentSlot::MainArm);
		if (slotIt != equipment->slots.end() && slotIt->second > 0) {
			auto* weapon = registry->GetComponent<WeaponComponent>(slotIt->second);
			if (weapon) {
				weaponPower = (weapon->minDamage + weapon->maxDamage) / 2.0f;
				weaponPower += stats->Strength * weapon->strScaling;
				weaponPower += stats->Dexterity * weapon->dexScaling;
				weaponPower += stats->Intelligence * weapon->intScaling;

				auto* itemComp = registry->GetComponent<ItemComponent>(slotIt->second);
				auto* holder = registry->GetComponent<SkillHolderComponent>(entityID);
				if (itemComp && holder && itemComp->primarySkillId != -1) {
					masteryLevel = holder->GetMastery(itemComp->primarySkillId);
				}
			}
		}
	}

	auto* mobAttack = registry->GetComponent<MobAttackComponent>(entityID);
	if (mobAttack) {
		weaponPower += mobAttack->attackDamage;
	}

	float masteryBonus = (std::min)(MASTERY_BONUS_CAP, MASTERY_BONUS_PER_LEVEL * (masteryLevel - 1));
	stats->Power = static_cast<int>((statBase + weaponPower) * (1.0f + masteryBonus) + 0.5f);
	stats->MagicPower = static_cast<int>(STAT_BASE + INTELLIGENCE_WEIGHT * stats->Intelligence +
		WISDOM_WEIGHT * stats->Wisdom + stats->MagicDamage + 0.5f);
}

float AttackInterval(Registry* registry, int entityID) {
	auto* mobAttack = registry->GetComponent<MobAttackComponent>(entityID);
	if (mobAttack) {
		return (std::max)(0.25f, mobAttack->attackSpeed);
	}

	auto* stats = registry->GetComponent<StatComponent>(entityID);
	auto* equipment = registry->GetComponent<EquipmentComponent>(entityID);

	float baseWindup = DEFAULT_ATTACK_INTERVAL;
	std::string scalingStat;
	float scalingFactor = 0.02f;
	if (equipment && stats) {
		auto slotIt = equipment->slots.find(EquipmentSlot::MainArm);
		if (slotIt != equipment->slots.end() && slotIt->second > 0) {
			auto* weapon = registry->GetComponent<WeaponComponent>(slotIt->second);
			if (weapon) {
				baseWindup = weapon->baseWindup;
				scalingStat = weapon->scalingStat;
				scalingFactor = weapon->scalingFactor;
			}
		}
	}

	int statValue = 0;
	if (stats) {
		if (scalingStat == "strength") statValue = stats->Strength;
		else if (scalingStat == "dexterity") statValue = stats->Dexterity;
		else if (scalingStat == "intelligence") statValue = stats->Intelligence;
	}

	float reduction = 1.0f - (1.0f / (1.0f + statValue * scalingFactor));
	reduction = (std::min)(reduction, 0.8f);
	return (std::max)(0.2f, baseWindup * (1.0f - reduction));
}

}

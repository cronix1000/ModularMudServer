#pragma once
#include <string>

class Registry;

struct MobAttackRoll {
	bool valid = false;
	float multiplier = 1.0f;
	std::string verb = "hits";
	std::string damageType = "physical";
};

namespace MobAttackBuilder {
	MobAttackRoll BuildAttack(Registry* registry, int mobID, int targetID);
}

#pragma once
#include <vector>
#include <string>

struct AttackIntentComponent {
	int victimID = -1;
	float damageMultiplier;
	std::string damageType;
	std::vector<std::string> addedTags;
	int attackSpeed = 2;
	bool onceAttack = false;

};
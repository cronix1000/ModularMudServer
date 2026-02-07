#pragma once
#include <string>

struct BaseStatComponent {
	int Health = 0;
	int MaxHealth = 0;
	int Armour = 0;
	int MagicDefense = 0;
	int Mana = 0;

	int Strength = 0;
	int Intelligence = 0;
	int Dexterity = 0;

	int AttackDamage = 0;
	int MagicDamage = 0;

	std::string damageType = "blunt";
};
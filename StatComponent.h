#pragma once
struct StatComponent
{
	int MaxHealth = 0;
	int Health = 0;
	int Armour = 0;
	int MagicDefense = 0;
	int Mana = 0;
	int Perception = 0;
	int attackSpeed = 3;

	int Strength = 0;
	int Intelligence = 0;
	int Wisdom = 0;
	int Dexterity = 0;

	int AttackDamage = 0;
	int MagicDamage = 0;

	std::string DamageType = "blunt";
	std::string DefenseType = "soft";

	StatComponent() = default;
};
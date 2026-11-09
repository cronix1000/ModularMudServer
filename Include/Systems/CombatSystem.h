#pragma once
#include <string>

struct GameContext;
struct CombatIntentComponent;

class CombatSystem
{
public:
	GameContext& ctx;
	CombatSystem(GameContext& g) : ctx(g) {}
	~CombatSystem() = default;
	void run();

private:
	void ProcessCombatIntent(int sourceID, const CombatIntentComponent& intent);
	void ProcessAttack(int sourceID, int targetID, float multiplier, const std::string& damageType,
	                   const std::string& attackVerb = "attacks", bool forceCritical = false,
	                   int skillID = -1);
	void ProcessHeal(int sourceID, int targetID, float healAmount);
	void ProcessBuff(int sourceID, int targetID, const std::string& buffType, float magnitude);
	void HandleDeath(int sourceID, int targetID, int skillID, int finalDamage);
	bool ShouldRenew(const CombatIntentComponent& intent);
	float RollCritChance(int sourceID);
	bool IsMagicDamage(const std::string& damageType);
};

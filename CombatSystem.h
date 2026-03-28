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
	void ProcessAttack(int sourceID, int targetID, float damage, const std::string& damageType, 
	                   const std::string& attackVerb = "attacks", bool isCritical = false);
	void ProcessHeal(int sourceID, int targetID, float healAmount);
	void ProcessBuff(int sourceID, int targetID, const std::string& buffType, float magnitude);
};
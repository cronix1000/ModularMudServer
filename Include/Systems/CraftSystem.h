#pragma once

#include <string>

struct GameContext;
struct CombatIntentComponent;

class CraftSystem
{
public:
	GameContext& ctx;
	CraftSystem(GameContext& g) : ctx(g) {}
	~CraftSystem() = default;
	void run();

private:
	void ProcessCraft(int sourceID, int skillID, float yieldMultiplier);

};
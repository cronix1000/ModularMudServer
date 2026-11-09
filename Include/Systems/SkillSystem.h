#pragma once
#include "GameContext.h"
#include "SkillContext.h"

class SkillSystem
{
public:
	GameContext& ctx;

	SkillSystem(GameContext& g) : ctx(g) {};
	~SkillSystem() = default;

	void Run(float dt);

private:

    void ProcessResult(int entity, SkillResult result, int targetId);

    bool CheckRequirements(int entityID, int skillID);
    void PayCosts(int entityID, int skillID);
    void ApplyCooldown(int entityID, int skillID, float modifier);
    void GrantXP(int entityID, int skillID, float amount);
    // The Core Logic
    void ExecuteScriptAndDispatch(int entityID, int skillID, int targetID);

};
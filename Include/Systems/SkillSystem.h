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

    // The Core Logic
    void ExecuteScriptAndDispatch(int entityID, int skillID, int targetID);

};

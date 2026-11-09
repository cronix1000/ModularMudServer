#pragma once
#include "GameContext.h"
#include "CombatComponents.h"
#include "SkillContext.h"

class AbilitySystem
{
public:
    GameContext& ctx;

    AbilitySystem(GameContext& g) : ctx(g) {};
    ~AbilitySystem() = default;

    void Run(float dt);

private:
    SkillContext ParseSkill();
    
};
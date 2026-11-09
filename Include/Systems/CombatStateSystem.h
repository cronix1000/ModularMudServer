#pragma once
#include "GameContext.h"

class CombatStateSystem {
public:
    GameContext& ctx;
    CombatStateSystem(GameContext& g) : ctx(g) {}
    ~CombatStateSystem() = default;
    void Run(float deltaTime);
};
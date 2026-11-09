#pragma once
#include "GameContext.h"

class TargetingSystem {
public:
    GameContext& ctx;
    TargetingSystem(GameContext& g) : ctx(g) {}
    ~TargetingSystem() = default;
    void Run(float deltaTime);
};
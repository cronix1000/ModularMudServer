#pragma once
#include <string>
#include "Entity.h"

// Skill.h
class Skill {
public:
    // 1. DATA (Loaded from JSON)
    std::string Name;
    int ManaCost;
    int Damage;
    float CooldownTime;

    virtual ~Skill() {}
};
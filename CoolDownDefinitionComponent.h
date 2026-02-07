#pragma once
struct CooldownStatsComponent {
    float windupTime = 0.0f; // Time BEFORE effect
    float recoveryTime = 0.0f; // Time AFTER effect (Busy)
    float cooldownTime = 0.0f; // Time until reusable
};
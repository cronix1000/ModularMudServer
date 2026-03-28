#pragma once

struct CombatStateComponent {
	int targetID = -1;              // Who we're fighting
	float attackTimer = 0.0f;       // Counts down to next attack
	float attackInterval = 1.0f;    // Derived from weapon windup
	bool active = true;
	bool isPlayer = false;
};
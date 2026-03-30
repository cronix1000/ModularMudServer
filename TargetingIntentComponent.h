#pragma once
#include <string>

struct TargetingIntentComponent {
	int sourceID;           // Who is targeting
	std::string targetName; // "goblin" or "2 2" for position
	int targetIndex = 0;    // 0 = auto, 1+ = specific
	bool isPositionTarget = false; // true for "meteor 2 2"
	int targetX = 0, targetY = 0;  // Position coordinates
	float maxRange = 0.0f;  // 0 = melee/same room
	bool requireLineOfSight = true;
	int resolvedTargetID = -1; // Filled by TargetingSystem
	float promptExpireTime = 5.0f; // Seconds to wait for player response
	float elapsedTime = 0.0f;      // Time since prompt sent
	int skillID = -1;
};
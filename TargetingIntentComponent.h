#pragma once
#include <string>
#include <vector>

struct TargetingIntentComponent {
    int sourceID;
    std::string targetName;
    int targetIndex = 0;
    bool isPositionTarget = false;
    int targetX = 0, targetY = 0;
    float maxRange = 0.0f;
    bool requireLineOfSight = true;
    int resolvedTargetID = -1;
    float promptExpireTime = 5.0f;
    float elapsedTime = 0.0f;
    int skillID = -1;

    // Disambiguation candidate list, populated when a query resolves to multiple entities.
    // The player picks one via `verb <N>` and the chosen EntityID is stored in resolvedTargetID.
    std::vector<int> candidates;
    // Original query tokens, kept so the system can re-resolve if the player refines.
    std::vector<std::string> queryTokens;
    // Room the candidates were scoped to at prompt time.
    int roomId = -1;
};
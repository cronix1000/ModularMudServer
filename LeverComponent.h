#pragma once
#include <string>

struct LeverComponent {
    std::string state = "up"; // "up", "down", "middle", etc.
    std::string event_trigger = "";
    int target_room = -1;
    int cooldown_seconds = 0;
    float last_used_time = 0.0f;
    bool can_be_reset = true;
    int uses_remaining = -1; // -1 for infinite uses
};
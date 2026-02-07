#pragma once
#include <string>
#include <map>

struct ShrineComponent {
    int heal_amount = 25;
    int mana_amount = 0;
    int cooldown_seconds = 300;
    float last_used_time = 0.0f;
    int max_uses_per_player = -1; // -1 for infinite
    std::map<int, int> player_use_count; // player_id -> use_count
    std::string blessing_type = "health"; // "health", "mana", "blessing", "curse_removal"
    std::string prayer_message = "You feel blessed by divine energy.";
};
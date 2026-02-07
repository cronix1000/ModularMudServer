#pragma once
#include <string>

struct ChestComponent {
    bool is_open = false;
    bool is_locked = false;
    std::string key_id = "";
    std::string loot_table = "";
    bool has_been_looted = false;
    int max_uses = 1;
    int uses_remaining = 1;
};
#pragma once
#include <vector>
struct InventoryComponent {
    // A list of the Entity IDs currently inside this inventory
    std::vector<int> items;
    int max_slots = 20;
    int current_weight = 0;
};
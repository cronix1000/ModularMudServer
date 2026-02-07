#pragma once
#include <string>
#include <nlohmann/json.hpp> 
#include <vector>
using json = nlohmann::json;

struct SavedItemData {
    std::string templateId; 
    nlohmann::json state; 
};

struct PlayerData {
    int id = 0;
    std::string name;
    std::string region;
    int room_id = 0;
    int x, y;
    std::vector<SavedItemData> items;
    nlohmann::json stats;




    PlayerData() = default;
};


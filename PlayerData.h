#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

struct SavedItemData {
    std::string templateId;
    json state;
};

struct PlayerData {
    int id = 0;
    int permission = 0;
    std::string name;
    std::string region;
    int room_id = 0;
    int x = 0, y = 0;
    int gold = 0;
    int bankBalance = 0;
    std::string classId;
    std::string raceId;
    int level = 1;
    std::vector<SavedItemData> items;
    json data;

    PlayerData() = default;
};


#pragma once


#pragma once
#include <vector>
#include <string>
#include <map>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct LootEntry {
    std::string itemTemplateID;
    int weight;
};

class LootFactory {
public:
    LootFactory() {}

    std::map<std::string, std::vector<LootEntry>> lootTables;

    void LoadLootTables(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        json data;
        file >> data;

        for (auto& [tableID, list] : data.items()) {
            std::vector<LootEntry> entries;
            for (auto& entry : list) {
                entries.push_back({ entry["id"], entry.value("weight", 1) });
            }
            lootTables[tableID] = entries;
        }
    }

    // Returns a random item ID from the specified table
    std::string RollTable(const std::string& tableID) {
        if (lootTables.find(tableID) == lootTables.end()) return "";

        auto& entries = lootTables[tableID];
        int totalWeight = 0;
        for (auto& e : entries) totalWeight += e.weight;

        std::uniform_int_distribution<int> dist(1, totalWeight);
        int roll = dist(rng);

        for (auto& e : entries) {
            roll -= e.weight;
            if (roll <= 0) return e.itemTemplateID;
        }
        return "";
    }

private:
    std::mt19937 rng{ std::random_device{}() };
};
#pragma once
#include <string>
#include <map>

struct TerrainDef {
    char symbol;
    std::string name;
    std::string color;
    std::string description;
    bool blocks_move = false;
    bool blocks_sight = false;
    int move_cost = 1;

    // Default constructor
    TerrainDef() = default;

    // Constructor with parameters
    TerrainDef(char symbol, const std::string& name, const std::string& color,
        bool blocks_move = false, bool blocks_sight = false, int move_cost = 1)
        :symbol(symbol), name(name), color(color), blocks_move(blocks_move),
        blocks_sight(blocks_sight), move_cost(move_cost) {
    }

    // Constructor for JSON loading (without description)
    TerrainDef(char symbol, const std::string& name, const std::string& color,
        const std::string& description, bool blocks_move = false,
        bool blocks_sight = false, int move_cost = 1)
        :symbol(symbol), name(name), color(color), description(description),
        blocks_move(blocks_move), blocks_sight(blocks_sight), move_cost(move_cost) {
    }
};// Global declarations - ADD THESE LINES:
extern TerrainDef VOID_TERRAIN;
extern std::map<char, TerrainDef> globalTerrain;
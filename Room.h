#pragma once
#include <string>
#include <vector>
#include <map>
#include <typeindex>
#include "Component.h"
#include "Direction.h"
#include "TerrainDef.h"


struct ExitData {
    int targetRoomID = -1;
    int destX = -1; // -1 indicates "Use Default/Calculated"
    int destY = -1;
};


struct RoomScriptData {
    std::string on_enter;
    std::string on_exit;
    std::string on_pulse; // For room-based traps or events
};
using Coordinate = std::pair<int, int>;

class Room
{
    int ID;
    int entityID;
    int width, height;
    std::vector<const TerrainDef*> grid;

public:
    std::map<Direction, ExitData> exits;
    std::map<char, TerrainDef> localTerrain;
    Coordinate spawn;
    RoomScriptData script;

    Room(int id, std::string name, std::string desc)
        : ID(id), Name(name), Description(desc) {
    }

    void SetEntityID(int id) {
        entityID = id;
    }

    int GetEnityID() {
        return entityID;
    }

    void AddExit(Direction dir, int roomID, int x = -1, int y = -1) {
        exits[dir] = { roomID, x, y };
    }

    // Return the full struct, not just the ID
    ExitData GetExit(Direction dir) {
        if (exits.find(dir) != exits.end()) {
            return exits[dir];
        }
        return { -1, -1, -1 }; // Invalid/No Exit
    }
    // cord.first is x, second is why
    bool HasLeftRoom(Coordinate coord) {
        if (coord.first >= width || coord.first <= 0 || 
            coord.second >= height || coord.second <= 0) {
            return true;
        }
        return false;
    }

    std::string Name;
    std::string Description;
	std::vector<int> entityIds; // id's of content enemies, npc's

    Room(int w, int h) : width(w), height(h) {
        // Initialize full of 'Empty' space
    }
    void InitializeGrid(int w, int h) {
        width = w;
        height = h;
        grid.clear();
        // Fill with VOID initially
        grid.resize(w * h, &VOID_TERRAIN);
    }
    // Set a tile using a pointer to an existing definition
    void SetTile(int x, int y, const TerrainDef* terrain) {
        if (IsValidCoord(x, y) && terrain != nullptr) {
            grid[y * width + x] = terrain;
        }
    }

    // Get the Definition directly
    const TerrainDef* GetTile(int x, int y) {
        if (!IsValidCoord(x, y)) return &VOID_TERRAIN; // Return safe default
        return grid[y * width + x];
    }

    void AddLocalTerrain(char character, TerrainDef terrain) {
        localTerrain[character] = terrain;
    }
    bool IsValidCoord(int x, int y) {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    int GetHeight() {
        return height;
    }
    int GetWidth() {
        return width;
    }

    int GetId() {
        return ID;
    }
    void LoadFromMap(const std::vector<std::string>& layoutLines) {
        int y = 0;

        for (const std::string& line : layoutLines) {
            int x = 0; // Tracks the actual grid X position

            // Loop through every character in the string
            for (char c : line) {

                // Check if it's a valid map character (ignore spacing)
                if (c != ' ') {
                    const TerrainDef* selectedTerrain = &VOID_TERRAIN;
                    if (localTerrain.count(c)) {
                        selectedTerrain = &localTerrain[c];
                    }
                    else if (globalTerrain.count(c)) {
                        selectedTerrain = &globalTerrain[c];
                    }
                    else {
                        if (globalTerrain.count('.')) selectedTerrain = &globalTerrain['.'];
                    }

                    // Assign the POINTER to the grid
                    SetTile(x, y, selectedTerrain);
                    x++;
                }

                // Stop if we hit the edge of the allocated grid
                if (x >= width) break;
            }

            // Move to the next row
            y++;
            if (y >= height) break;
        }
    }
    bool HasGrid() {
        if (grid.empty()) {
            return false;
        }
        return true;
    }
    // Now we check properties directly on the pointer!
    bool IsWall(int x, int y) {
        const TerrainDef* t = GetTile(x, y);
        return t->blocks_move;
    }
};
    
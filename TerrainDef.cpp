#include "TerrainDef.h"

// Define the global variables
TerrainDef VOID_TERRAIN{' ', "Void", "&x", "Empty void space", true, true, 999};
std::map<char, TerrainDef> globalTerrain;
std::unordered_map<std::string, std::map<char, TerrainDef>> regionTerrain;

namespace {

const TerrainDef& LookupTerrain(char symbol, const std::string* regionId) {
    if (regionId && !regionId->empty()) {
        auto regionIt = regionTerrain.find(*regionId);
        if (regionIt != regionTerrain.end()) {
            auto localIt = regionIt->second.find(symbol);
            if (localIt != regionIt->second.end()) {
                return localIt->second;
            }
        }
    }
    auto globalIt = globalTerrain.find(symbol);
    if (globalIt != globalTerrain.end()) {
        return globalIt->second;
    }
    auto dotIt = globalTerrain.find('.');
    if (dotIt != globalTerrain.end()) {
        return dotIt->second;
    }
    return VOID_TERRAIN;
}

} // namespace

const TerrainDef& GetTerrainFor(char symbol, const std::string* regionId) {
    return LookupTerrain(symbol, regionId);
}

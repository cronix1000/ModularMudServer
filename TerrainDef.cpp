#include "TerrainDef.h"

// Define the global variables
TerrainDef VOID_TERRAIN{' ', "Void", "&x", "Empty void space", true, true, 999};
std::map<char, TerrainDef> globalTerrain;

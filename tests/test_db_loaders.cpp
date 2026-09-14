// Standalone smoke test for SQLiteDatabase world loaders.
// Build: included in CMakeLists as a non-default target.
// Run from the project root: ./test_db_loaders ../ModularMudServer/mud.db

#include "SQLiteDatabase.h"
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static int errors = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; ++errors; } else { std::cout << "PASS: " << msg << std::endl; } } while(0)

int main(int argc, char** argv) {
    std::string dbPath = argc > 1 ? argv[1] : "../ModularMudServer/mud.db";

    SQLiteDatabase db;
    if (!db.Connect(dbPath)) {
        std::cerr << "Connect failed for " << dbPath << std::endl;
        return 2;
    }
    std::cout << "Connected to " << dbPath << std::endl;

    // Terrain
    CHECK(db.LoadTerrain(), "LoadTerrain returned true");

    // Items
    json items = db.LoadItems("default");
    std::cout << "Items is_object=" << items.is_object() << ", keys=" << items.size() << std::endl;
    CHECK(items.is_object() && items.size() >= 4, "LoadItems returns >=4 item templates");
    if (items.contains("stick")) {
        json stick = items["stick"];
        std::cout << "stick: " << stick.dump() << std::endl;
        CHECK(stick.contains("name"), "stick has name");
        CHECK(stick.contains("components"), "stick has components (merged from components_json)");
        CHECK(stick["components"].is_object(), "stick.components is object");
    }

    // Mobs
    json mobs = db.LoadMobs("default");
    std::cout << "Mobs: keys=" << mobs.size() << std::endl;
    CHECK(mobs.is_object() && mobs.size() >= 5, "LoadMobs returns >=5 mob templates");
    if (mobs.contains("goblin")) {
        json goblin = mobs["goblin"];
        std::cout << "goblin: " << goblin.dump() << std::endl;
        CHECK(goblin.contains("stat"), "goblin has stat object (merged)");
        CHECK(goblin["stat"].is_object(), "goblin.stat is object");
        CHECK(goblin.contains("attack_patterns"), "goblin has attack_patterns array (from *_json)");
        CHECK(goblin["attack_patterns"].is_array(), "goblin.attack_patterns is array");
    }

    // Interactables
    json interactables = db.LoadInteractables("default");
    std::cout << "Interactables: keys=" << interactables.size() << std::endl;
    CHECK(interactables.size() >= 3, "LoadInteractables returns >=3 templates");
    if (interactables.contains("healing_shrine")) {
        std::cout << "healing_shrine: " << interactables["healing_shrine"].dump() << std::endl;
    }

    // Skills
    json skills = db.LoadSkills("default");
    std::cout << "Skills: " << skills.dump() << std::endl;
    CHECK(skills.contains("skill_categories"), "skills has skill_categories");
    CHECK(skills.contains("skills"), "skills has skills");
    CHECK(skills["skill_categories"].size() >= 3, "skill_categories >= 3");
    CHECK(skills["skills"].size() >= 3, "skills >= 3");

    // Loot tables
    json loot = db.LoadLootTables("default");
    std::cout << "Loot: " << loot.dump() << std::endl;

    // Dialogues
    json dialogues = db.LoadDialogues("default");
    std::cout << "Dialogues: " << dialogues.dump() << std::endl;
    CHECK(dialogues.size() >= 2, "LoadDialogues returns >=2 entries");

    // Region + rooms
    CHECK(db.RegionExists("default", "floor1"), "Region floor1 exists");
    json floorSettings;
    CHECK(db.LoadRegionFloorSettings("default", "floor1", floorSettings), "LoadRegionFloorSettings floor1");
    std::cout << "floor1 settings: " << floorSettings.dump() << std::endl;

    auto roomIds = db.LoadRoomIds("default", "floor1");
    std::cout << "floor1 room ids: ";
    for (int r : roomIds) std::cout << r << " ";
    std::cout << std::endl;
    CHECK(!roomIds.empty(), "floor1 has rooms");

    for (int rid : roomIds) {
        json rData;
        if (!db.LoadRoomJson("default", "floor1", rid, rData)) {
            std::cerr << "FAIL: LoadRoomJson " << rid << std::endl;
            ++errors;
            continue;
        }
        std::cout << "Room " << rid << ":\n" << rData.dump(2) << std::endl;
        CHECK(rData["id"] == rid, "room id matches");
        if (rData.contains("spawns")) {
            CHECK(rData["spawns"].is_array(), "room spawns is array");
            CHECK(rData.contains("spawn_legend"), "room has spawn_legend");
        }
    }

    db.Disconnect();
    std::cout << "=== " << (errors ? "FAILED" : "ALL PASS") << " === (errors=" << errors << ")" << std::endl;
    return errors ? 1 : 0;
}

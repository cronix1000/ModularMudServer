// Standalone tests for EntityResolver / NameComponent::MatchesPhrase.
// Build: included in CMakeLists as a non-default target.
// Run: ./test_entity_resolver

#include "EntityResolver.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "InventoryComponent.h"
#include "Registry.h"

#include <iostream>
#include <string>
#include <vector>

static int errors = 0;
static int passed = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; ++errors; } \
    else { std::cout << "PASS: " << msg << std::endl; ++passed; } \
} while (0)

static EntityID spawn(Registry& r, const std::string& name, int x, int y, int roomId, bool carryInventory = false) {
    EntityID e = r.CreateEntity();
    r.AddComponent<NameComponent>(e, NameComponent(name));
    r.AddComponent<PositionComponent>(e, PositionComponent{x, y, roomId});
    if (carryInventory) {
        r.AddComponent<InventoryComponent>(e, InventoryComponent{});
    }
    return e;
}

static std::vector<std::string> tokens(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int main() {
    // --- Case 1: exact single token ---
    {
        Registry r;
        EntityID sword = spawn(r, "sword", 0, 0, 1);
        EntityResolver res(&r);
        auto hits = res.FindInRoom(1, tokens("sword"));
        CHECK(hits.size() == 1 && hits[0] == sword, "exact single token 'sword' matches one entity");
    }

    // --- Case 2: multi-token in-order ---
    {
        Registry r;
        EntityID rusty = spawn(r, "rusty iron sword", 5, 5, 1);
        spawn(r, "elven short sword", 6, 6, 1);
        EntityResolver res(&r);
        auto hits = res.FindInRoom(1, tokens("rusty sword"));
        CHECK(hits.size() == 1 && hits[0] == rusty, "multi-token in-order 'rusty sword' matches 'rusty iron sword'");
    }

    // --- Case 3: multi-token out-of-order ---
    {
        Registry r;
        spawn(r, "rusty iron sword", 0, 0, 1);
        EntityResolver res(&r);
        auto hits = res.FindInRoom(1, tokens("sword rusty"));
        CHECK(hits.empty(), "out-of-order 'sword rusty' does not match 'rusty iron sword'");
    }

    // --- Case 4: room scoping ---
    {
        Registry r;
        EntityID here = spawn(r, "sword", 0, 0, 1);
        spawn(r, "sword", 0, 0, 2);
        EntityResolver res(&r);
        auto hits = res.FindInRoom(1, tokens("sword"));
        CHECK(hits.size() == 1 && hits[0] == here, "only entities in target room are returned");
    }

    // --- Case 5: ShapeAround restricts to 3x3 neighborhood ---
    {
        Registry r;
        EntityID adjacent = spawn(r, "sword", 1, 0, 1);
        spawn(r, "sword", 5, 5, 1);
        EntityResolver res(&r);
        auto hits = res.FindByShape(1, 0, 0, tokens("sword"), EntityResolver::ShapeAround());
        CHECK(hits.size() == 1 && hits[0] == adjacent, "ShapeAround excludes entities 2+ tiles away");
    }

    // --- Case 6: ShapeAtTile matches only the center tile ---
    {
        Registry r;
        EntityID here = spawn(r, "chest", 3, 3, 1);
        spawn(r, "chest", 4, 3, 1);
        EntityResolver res(&r);
        auto hits = res.FindByShape(1, 3, 3, tokens("chest"), EntityResolver::ShapeAtTile());
        CHECK(hits.size() == 1 && hits[0] == here, "ShapeAtTile matches only the exact tile");
    }

    // --- Case 7: ScanInShape returns all in shape, no name filter ---
    {
        Registry r;
        EntityID a = spawn(r, "goblin", 1, 0, 1);
        EntityID b = spawn(r, "chest", 0, 1, 1);
        spawn(r, "dragon", 10, 10, 1);
        EntityResolver res(&r);
        auto hits = res.ScanInShape(1, 0, 0, EntityResolver::ShapeAround());
        CHECK(hits.size() == 2, "ScanInShape returns 2 adjacent entities (no name filter)");
        bool hasA = false, hasB = false;
        for (auto h : hits) {
            if (h == a) hasA = true;
            if (h == b) hasB = true;
        }
        CHECK(hasA && hasB, "ScanInShape includes both goblin and chest");
    }

    // --- Case 8: Inventory-holding entities are excluded ---
    {
        Registry r;
        EntityID carried = spawn(r, "sword", 0, 0, 1, true);
        EntityResolver res(&r);
        auto hits = res.FindInRoom(1, tokens("sword"));
        CHECK(hits.empty(), "entities with InventoryComponent are excluded from lookups");
    }

    std::cout << "--- " << passed << " passed, " << errors << " failed ---" << std::endl;
    return errors == 0 ? 0 : 1;
}
#pragma once
#include "IDatabase.h"
#include "ItemFactory.h"
#include "MobFactory.h"
#include "LootFactory.h"
#include "DialogueFactory.h"
#include "PlayerFactory.h"
#include "SkillFactory.h"
#include "InteractableFactory.h"

class FactoryManager {
public:
    // References to the shared game state
    GameContext& ctx;

    // The sub-factories
    ItemFactory items;
    MobFactory mobs;
    LootFactory loot;
    DialogueFactory dialogue;
    PlayerFactory player;
    SkillFactory skills;
    InteractableFactory interactables;

    FactoryManager(GameContext& g)
        : ctx(g), items(g), mobs(g), loot(), dialogue(g), player(g), skills(g), interactables(g) {
    }

    // One function to load the entire game database.
    // Source of truth: the world.* / players.* tables in MUD_DATABASE_URL.
    void LoadAllData() {
        std::cout << "Loading Game Database from Postgres..." << std::endl;

        const std::string worldId = "default";

        if (!ctx.db) {
            std::cerr << "[FactoryManager] No database available; aborting LoadAllData." << std::endl;
            return;
        }

        ctx.db->LoadTerrain();

        items.LoadItemTemplatesFromJson(ctx.db->LoadItems(worldId));
        mobs.LoadMobTemplatesFromJson(ctx.db->LoadMobs(worldId));
        interactables.LoadInteractableTemplatesFromJson(ctx.db->LoadInteractables(worldId));
        skills.LoadSkillsFromJson(ctx.db->LoadSkills(worldId));
        loot.LoadLootTablesFromJson(ctx.db->LoadLootTables(worldId));
        dialogue.LoadDialogueAndVoicesFromJson(ctx.db->LoadDialogues(worldId));

        std::cout << "Database Loaded Successfully." << std::endl;
    }

    bool TemplateExists(const std::string& id) {
        if (items.itemTemplates.count(id)) return true;
        if (mobs.mobTemplates.count(id)) return true;
        if (loot.lootTables.count(id)) return true;
        return false;
    }

};
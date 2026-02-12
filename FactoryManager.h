#pragma once
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

    // One function to load the entire game database
    void LoadAllData() {
        std::cout << "Loading Game Database..." << std::endl;

        // Order matters if they cross-reference
        items.LoadItemTemplatesFromLua();
        loot.LoadLootTables("loot_drops.json");
        dialogue.LoadDialogueAndVoices("dialogue.json");
        mobs.LoadMobTemplatesFromLua();
        interactables.LoadInteractableTemplatesFromLua();
        skills.LoadSkillsFromLua();

        std::cout << "Database Loaded Successfully." << std::endl;
    }

    bool TemplateExists(const std::string& id) {
        if (items.itemTemplates.count(id)) return true;
        if (mobs.mobTemplates.count(id)) return true;
        if (loot.lootTables.count(id)) return true;
        return false;
    }

};
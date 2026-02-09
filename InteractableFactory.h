#pragma once
#include "GameContext.h"
#include <fstream>
#include <iostream>
#include <map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "VisualComponent.h"
#include "PositionComponent.h"
#include "NameComponent.h"
#include "DescriptionComponent.h"
#include "ScriptComponent.h"
#include "PortalComponent.h"
#include "ChestComponent.h"
#include "DoorComponent.h"
#include "LeverComponent.h"
#include "ShrineComponent.h"
#include "InteractableComponent.h"
#include "InteractableContext.h"
#include "Registry.h"
#include "ScriptManager.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// --- The Template (Static Blueprint) ---
struct InteractableTemplate {
    std::string name;
    std::string symbol;
    std::string color;
    std::string description;
    std::string scriptPath; // Path to the Lua script for this interactable
    std::string interactableType; // "portal", "chest", "lever", "door", etc.
    
    json extra; // Raw data for specific components
};

class InteractableFactory {
public:
    GameContext& ctx;
    std::map<std::string, InteractableTemplate> interactableTemplates;

    InteractableFactory(GameContext& g) : ctx(g) {}

    void LoadInteractableTemplatesFromLua() {
         ctx.scripts->lua.script_file("scripts/interactables/interactables_master.lua");

        // 2. Get the Global Table
        sol::table interactablesTable = ctx.scripts->lua["interactables"];
		std::cout << "Loading interactable templates from Lua..." << std::endl;
        
        if (!interactablesTable.valid()) {
            std::cerr << "[Error] Global 'interactables' table not found in Lua state!" << std::endl;
            return;
        }

    int count = 0;

        // 3. Iterate over every skill defined in Lua
        for (auto& kv : interactablesTable) {
            // key = "slash", "fireball"
            std::string key = kv.first.as<std::string>();
            // value = The table containing {name="...", cooldown=...}
            sol::table skillData = kv.second;
                
            LoadSingleInteractableFromLua(key, skillData);
            count++;
        }
        
        std::cout << "Loaded " << interactableTemplates.size() << " interactable templates from Lua." << std::endl;
    }

    int CreateInteractable(std::string templateID, json overrides = json::object(), int x = 0, int y = 0, int roomId = -1) {
        auto it = interactableTemplates.find(templateID);
        if (it == interactableTemplates.end()) {
            std::cout << "Interactable template not found: " << templateID << std::endl;
            return -1;
        }

        const auto& tpl = it->second;
        int id = ctx.registry->CreateEntity();

        // Get template data from Lua, potentially modified by overrides
        auto templateData = GetTemplateDataWithOverrides(templateID, overrides);

        // 1. Basic Components (use potentially overridden data)
        ctx.registry->AddComponent<NameComponent>(id, NameComponent(templateData.name));
        ctx.registry->AddComponent<DescriptionComponent>(id, DescriptionComponent{ templateData.description });
        ctx.registry->AddComponent<VisualComponent>(id, VisualComponent{ templateData.symbol, templateData.color });
        
        // 2. Position Component
        if (roomId != -1) {
            ctx.registry->AddComponent<PositionComponent>(id, PositionComponent{ x, y, roomId });
        }

        // 3. Script Component (if script exists)
        if (!tpl.scriptPath.empty()) {
            ScriptComponent script;
            script.scripts_path["on_use"] = tpl.scriptPath;
            script.scripts_path["on_create"] = tpl.scriptPath;
            ctx.registry->AddComponent<ScriptComponent>(id, script);
        }

        // 4. Type-specific components (store overrides in components)
        AttachTypeSpecificComponents(id, templateData, overrides);

        // 5. Notify ScriptManager to execute creation script
        if (!tpl.scriptPath.empty()) {
            // ScriptManager will handle this during system update
            // For now, we could add a CreationIntentComponent or handle in InteractionSystem
        }

        return id;
    }



private:
    void AttachTypeSpecificComponents(int id, const InteractableTemplate& tpl, json overrides) {
        // Handle specific interactable types
        if (tpl.interactableType == "portal") {
            PortalComponent portal;
            portal.destination_room = tpl.extra.value("destination_room", -1);
            portal.direction_command = tpl.extra.value("direction_command", "enter");
            portal.is_open = tpl.extra.value("is_open", true);
            portal.is_locked = tpl.extra.value("is_locked", false);
            portal.key_id = tpl.extra.value("key_id", -1);
            
            // Apply overrides
            if (overrides.contains("destination")) {
                auto dest = overrides["destination"];
                portal.destination_room = dest.value("target_room", portal.destination_room);
            }
            
            ctx.registry->AddComponent<PortalComponent>(id, portal);
        }
        else if (tpl.interactableType == "chest") {
            ChestComponent chest;
            chest.is_open = tpl.extra.value("is_open", false);
            chest.is_locked = tpl.extra.value("is_locked", false);
            chest.key_id = tpl.extra.value("key_id", "");
            chest.loot_table = tpl.extra.value("loot_table", "");
            chest.max_uses = tpl.extra.value("max_uses", 1);
            chest.uses_remaining = chest.max_uses;
            
            // Apply overrides
            if (overrides.contains("components")) {
                auto comp = overrides["components"];
                if (comp.contains("is_locked")) chest.is_locked = comp["is_locked"];
                if (comp.contains("key_id")) chest.key_id = comp["key_id"];
                if (comp.contains("loot_table")) chest.loot_table = comp["loot_table"];
                if (comp.contains("max_uses")) chest.max_uses = comp["max_uses"];
            }
            
            ctx.registry->AddComponent<ChestComponent>(id, chest);
        }
        else if (tpl.interactableType == "door") {
            DoorComponent door;
            door.is_open = tpl.extra.value("is_open", false);
            door.is_locked = tpl.extra.value("is_locked", true);
            door.key_id = tpl.extra.value("key_id", "");
            door.destination_room = tpl.extra.value("destination_room", -1);
            door.destination_x = tpl.extra.value("destination_x", -1);
            door.destination_y = tpl.extra.value("destination_y", -1);
            door.open_message = tpl.extra.value("open_message", "The door creaks open.");
            door.close_message = tpl.extra.value("close_message", "The door slams shut.");
            
            // Apply overrides
            if (overrides.contains("destination")) {
                auto dest = overrides["destination"];
                door.destination_room = dest.value("target_room", door.destination_room);
                door.destination_x = dest.value("dest_x", door.destination_x);
                door.destination_y = dest.value("dest_y", door.destination_y);
            }
            if (overrides.contains("components")) {
                auto comp = overrides["components"];
                if (comp.contains("is_locked")) door.is_locked = comp["is_locked"];
                if (comp.contains("key_id")) door.key_id = comp["key_id"];
                if (comp.contains("is_open")) door.is_open = comp["is_open"];
            }
            
            ctx.registry->AddComponent<DoorComponent>(id, door);
        }
        else if (tpl.interactableType == "lever" || tpl.interactableType == "switch") {
            LeverComponent lever;
            lever.state = tpl.extra.value("state", "up");
            lever.event_trigger = tpl.extra.value("event_trigger", "");
            lever.target_room = tpl.extra.value("target_room", -1);
            lever.cooldown_seconds = tpl.extra.value("cooldown", 0);
            lever.can_be_reset = tpl.extra.value("can_be_reset", true);
            lever.uses_remaining = tpl.extra.value("uses_remaining", -1);
            
            // Apply overrides
            if (overrides.contains("components")) {
                auto comp = overrides["components"];
                if (comp.contains("state")) lever.state = comp["state"];
                if (comp.contains("event_trigger")) lever.event_trigger = comp["event_trigger"];
                if (comp.contains("target_room")) lever.target_room = comp["target_room"];
                if (comp.contains("cooldown")) lever.cooldown_seconds = comp["cooldown"];
            }
            
            ctx.registry->AddComponent<LeverComponent>(id, lever);
        }
        else if (tpl.interactableType == "shrine" || tpl.interactableType == "altar") {
            ShrineComponent shrine;
            shrine.heal_amount = tpl.extra.value("heal_amount", 25);
            shrine.mana_amount = tpl.extra.value("mana_amount", 0);
            shrine.cooldown_seconds = tpl.extra.value("cooldown", 300);
            shrine.max_uses_per_player = tpl.extra.value("max_uses_per_player", -1);
            shrine.blessing_type = tpl.extra.value("blessing_type", "health");
            shrine.prayer_message = tpl.extra.value("prayer_message", "You feel blessed by divine energy.");
            
            // Apply overrides
            if (overrides.contains("components")) {
                auto comp = overrides["components"];
                if (comp.contains("heal_amount")) shrine.heal_amount = comp["heal_amount"];
                if (comp.contains("mana_amount")) shrine.mana_amount = comp["mana_amount"];
                if (comp.contains("cooldown")) shrine.cooldown_seconds = comp["cooldown"];
                if (comp.contains("max_uses_per_player")) shrine.max_uses_per_player = comp["max_uses_per_player"];
                if (comp.contains("blessing_type")) shrine.blessing_type = comp["blessing_type"];
            }
            
            ctx.registry->AddComponent<ShrineComponent>(id, shrine);
        }
        else {
            // Generic interactable - store all extra data in InteractableComponent
            InteractableComponent interactable;
            interactable.interaction_type = tpl.interactableType;
            interactable.cooldown_seconds = tpl.extra.value("cooldown", 0);
            interactable.uses_remaining = tpl.extra.value("uses_remaining", -1);
            
            // Store all extra data in the appropriate maps
            for (auto& [key, value] : tpl.extra.items()) {
                if (value.is_string()) {
                    interactable.custom_data[key] = value.get<std::string>();
                } else if (value.is_number_integer()) {
                    interactable.int_data[key] = value.get<int>();
                } else if (value.is_number_float()) {
                    interactable.float_data[key] = value.get<float>();
                } else if (value.is_boolean()) {
                    interactable.bool_data[key] = value.get<bool>();
                }
            }
            
            // Apply overrides to generic component
            if (overrides.contains("components")) {
                auto comp = overrides["components"];
                for (auto& [key, value] : comp.items()) {
                    if (value.is_string()) {
                        interactable.custom_data[key] = value.get<std::string>();
                    } else if (value.is_number_integer()) {
                        interactable.int_data[key] = value.get<int>();
                    } else if (value.is_number_float()) {
                        interactable.float_data[key] = value.get<float>();
                    } else if (value.is_boolean()) {
                        interactable.bool_data[key] = value.get<bool>();
                    }
                }
            }
            
            ctx.registry->AddComponent<InteractableComponent>(id, interactable);
        }
    }

    InteractableTemplate GetTemplateDataWithOverrides(const std::string& templateID, json overrides);
    void LoadSingleInteractableFromLua(std::string& key, sol::table& interactData);
};

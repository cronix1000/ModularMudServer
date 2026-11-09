#include "InteractableFactory.h"
#include "SafeJson.h"
#include "InteractableComponents.h"
#include "InventoryComponents.h"
#include "GameContext.h"
#include "MetaComponent.h"
#include "MetaRegistry.h"

void InteractableFactory::LoadInteractableTemplatesFromJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Error] Could not open interactables.json: " << path << std::endl;
        return;
    }

    json data;
    try {
        file >> data;
    }
    catch (const json::parse_error& e) {
        std::cerr << "[Error] Failed to parse interactables.json: " << e.what() << std::endl;
        return;
    }

    LoadInteractableTemplatesFromJson(data);
}

void InteractableFactory::LoadInteractableTemplatesFromJson(const json& data) {
    std::cout << "Loading Interactable Templates from JSON..." << std::endl;
    int count = 0;
    for (auto& [key, j] : data.items()) {
        LoadSingleInteractableFromJSON(key, j);
        count++;
    }
    std::cout << "[InteractableFactory] Loaded " << count << " interactable templates." << std::endl;
}

void InteractableFactory::LoadSingleInteractableFromJSON(const std::string& key, const json& j) {
    InteractableTemplate tpl;
    tpl.id = key;
    tpl.name = SafeValue(j, "name", "Unknown Interactable");
    tpl.symbol = SafeValue(j, "char", "?");
    tpl.color = SafeValue(j, "color", "&w");
    tpl.description = SafeValue(j, "description", "");
    tpl.script = SafeValue(j, "script", "");
    tpl.components = SafeValue(j, "components", json::object());

    interactableTemplates[key] = tpl;
    std::cout << "  Loaded: " << key << " (" << tpl.name << ")" << std::endl;
}

int InteractableFactory::CreateInteractable(std::string templateID, json overrides, int x, int y, int roomId) {
    auto it = interactableTemplates.find(templateID);
    if (it == interactableTemplates.end()) {
        std::cerr << "Interactable template not found: " << templateID << std::endl;
        return -1;
    }

    const auto& tpl = it->second;
    int id = ctx.registry->CreateEntity();

    std::string name = SafeValue(overrides, "name", tpl.name);
    std::string desc = SafeValue(overrides, "description", tpl.description);
    std::string sym = SafeValue(overrides, "char", tpl.symbol);
    std::string col = SafeValue(overrides, "color", tpl.color);

    ctx.registry->AddComponent<NameComponent>(id, NameComponent{ name });
    ctx.registry->AddComponent<DescriptionComponent>(id, DescriptionComponent{ desc });
    ctx.registry->AddComponent<VisualComponent>(id, VisualComponent{ sym, col });

    if (roomId != -1) {
        ctx.registry->AddComponent<PositionComponent>(id, PositionComponent{ x, y, roomId });
    }

    if (!tpl.script.empty()) {
        ScriptComponent script;
        script.scripts_path["on_use"] = tpl.script;
        script.scripts_path["on_create"] = tpl.script;
        ctx.registry->AddComponent<ScriptComponent>(id, script);
    }

    json finalComponents = tpl.components;
    if (overrides.contains("components")) {
        for (auto& [key, value] : overrides["components"].items()) {
            finalComponents[key] = value;
        }
    }

    AttachComponents(id, finalComponents, overrides);

    if (!ctx.registry->HasComponent<InteractableComponent>(id)) {
        InteractableComponent marker;
        marker.interaction_type = "generic";
        ctx.registry->AddComponent<InteractableComponent>(id, marker);
    }

    if (finalComponents.contains("meta") && finalComponents["meta"].is_object()) {
        json meta = finalComponents["meta"];
        MetaRegistry::ApplyDefaults("interactable", meta);
        ctx.registry->AddComponent<MetaComponent>(id, MetaComponent{ meta });
    }

    if (finalComponents.contains("messages") && finalComponents["messages"].is_object()) {
        const json& messagesJson = finalComponents["messages"];
        InteractionOverrideComponent ioc;
        for (auto it = messagesJson.begin(); it != messagesJson.end(); ++it) {
            if (it.value().is_array()) {
                for (auto& m : it.value()) {
                    if (m.is_string()) ioc.overrides[it.key()].push_back(m.get<std::string>());
                }
            }
        }
        if (!ioc.overrides.empty()) {
            ctx.registry->AddComponent<InteractionOverrideComponent>(id, ioc);
        }
    }

    return id;
}

void InteractableFactory::AttachComponents(int id, const json& components, json overrides) {
    for (auto& [compName, compData] : components.items()) {
        if (compName == "meta" || compName == "messages") continue;
        if (compName == "portal") {
            PortalComponent portal;
            portal.destination_room = SafeValue(compData, "destination_room", -1);
            portal.direction_command = SafeValue(compData, "direction_command", "enter");
            portal.is_open = SafeValue(compData, "is_open", true);
            portal.is_locked = SafeValue(compData, "is_locked", false);
            portal.key_id = SafeValue(compData, "key_id", -1);
            ctx.registry->AddComponent<PortalComponent>(id, portal);
            InteractableComponent marker;
            marker.interaction_type = "portal";
            if (compData.contains("max_uses")) {
                marker.uses_remaining = SafeValue(compData, "max_uses", -1);
            }
            ctx.registry->AddComponent<InteractableComponent>(id, marker);
        }
        else if (compName == "chest") {
            ChestComponent chest;
            chest.is_open = SafeValue(compData, "is_open", false);
            chest.is_locked = SafeValue(compData, "is_locked", false);
            chest.key_id = SafeValue(compData, "key_id", -1);
            chest.loot_table = SafeValue(compData, "loot_table", "");
            chest.max_uses = SafeValue(compData, "max_uses", 1);
            chest.uses_remaining = chest.max_uses;
            ctx.registry->AddComponent<ChestComponent>(id, chest);
            InteractableComponent marker;
            marker.interaction_type = "chest";
            marker.uses_remaining = SafeValue(compData, "max_uses", 1);
            ctx.registry->AddComponent<InteractableComponent>(id, marker);
        }
        else if (compName == "door") {
            DoorComponent door;
            door.is_open = SafeValue(compData, "is_open", false);
            door.is_locked = SafeValue(compData, "is_locked", true);
            door.key_id = SafeValue(compData, "key_id", "");
            door.destination_room = SafeValue(compData, "destination_room", -1);
            door.destination_x = SafeValue(compData, "destination_x", -1);
            door.destination_y = SafeValue(compData, "destination_y", -1);
            door.open_message = SafeValue(compData, "open_message", "The door creaks open.");
            door.close_message = SafeValue(compData, "close_message", "The door slams shut.");
            ctx.registry->AddComponent<DoorComponent>(id, door);
            InteractableComponent marker;
            marker.interaction_type = "door";
            ctx.registry->AddComponent<InteractableComponent>(id, marker);
        }
        else if (compName == "lever") {
            LeverComponent lever;
            lever.state = SafeValue(compData, "state", "up");
            lever.event_trigger = SafeValue(compData, "event_trigger", "");
            lever.target_room = SafeValue(compData, "target_room", -1);
            lever.cooldown_seconds = SafeValue(compData, "cooldown", 0);
            lever.can_be_reset = SafeValue(compData, "can_be_reset", true);
            lever.uses_remaining = SafeValue(compData, "uses_remaining", -1);
            ctx.registry->AddComponent<LeverComponent>(id, lever);
            InteractableComponent marker;
            marker.interaction_type = "lever";
            marker.cooldown_seconds = SafeValue(compData, "cooldown", 0);
            marker.uses_remaining = SafeValue(compData, "uses_remaining", -1);
            ctx.registry->AddComponent<InteractableComponent>(id, marker);
        }
        else if (compName == "healing") {
            ShrineComponent shrine;
            shrine.heal_amount = SafeValue(compData, "heal_amount", 25);
            shrine.mana_amount = SafeValue(compData, "mana_amount", 0);
            shrine.cooldown_seconds = SafeValue(compData, "cooldown", 300);
            shrine.max_uses_per_player = SafeValue(compData, "max_uses_per_player", -1);
            shrine.blessing_type = SafeValue(compData, "blessing_type", "health");
            shrine.prayer_message = SafeValue(compData, "prayer_message", "You feel blessed by divine energy.");
            ctx.registry->AddComponent<ShrineComponent>(id, shrine);
            InteractableComponent marker;
            marker.interaction_type = "healing";
            marker.cooldown_seconds = SafeValue(compData, "cooldown", 300);
            marker.uses_remaining = SafeValue(compData, "max_uses_per_player", -1);
            ctx.registry->AddComponent<InteractableComponent>(id, marker);
        }
        else if (compName == "inventory") {
            InteractableComponent interactable;
            interactable.interaction_type = "container";
            interactable.cooldown_seconds = SafeValue(compData, "cooldown", 0);
            interactable.uses_remaining = SafeValue(compData, "max_uses", -1);
            ctx.registry->AddComponent<InteractableComponent>(id, interactable);
        }
        else if (compName == "loot") {
            InteractableComponent interactable;
            interactable.interaction_type = "loot";
            interactable.custom_data["loot_table"] = SafeValue(compData, "table", "");
            interactable.int_data["max_uses"] = SafeValue(compData, "max_uses", 1);
            ctx.registry->AddComponent<InteractableComponent>(id, interactable);
        }
        else if (compName == "crafting") {
            CraftingStationComponent station;
            station.isOccupied = false;
            ctx.registry->AddComponent<CraftingStationComponent>(id, station);

            InteractableComponent marker;
            marker.interaction_type = "crafting";
            marker.cooldown_seconds = SafeValue(compData, "cooldown", 0);
            marker.uses_remaining = SafeValue(compData, "max_uses", -1);
            ctx.registry->AddComponent<InteractableComponent>(id, marker);
        }
        else {
            InteractableComponent interactable;
            interactable.interaction_type = compName;
            for (auto& [key, value] : compData.items()) {
                if (value.is_string()) {
                    interactable.custom_data[key] = value.get<std::string>();
                }
                else if (value.is_number_integer()) {
                    interactable.int_data[key] = value.get<int>();
                }
                else if (value.is_number_float()) {
                    interactable.float_data[key] = value.get<float>();
                }
                else if (value.is_boolean()) {
                    interactable.bool_data[key] = value.get<bool>();
                }
            }
            ctx.registry->AddComponent<InteractableComponent>(id, interactable);
        }
    }
}
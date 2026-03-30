#include "InteractableFactory.h"
#include "GameContext.h"

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
    tpl.name = j.value("name", "Unknown Interactable");
    tpl.symbol = j.value("char", "?");
    tpl.color = j.value("color", "&w");
    tpl.description = j.value("description", "");
    tpl.script = j.value("script", "");
    tpl.components = j.value("components", json::object());

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

    std::string name = overrides.value("name", tpl.name);
    std::string desc = overrides.value("description", tpl.description);
    std::string sym = overrides.value("char", tpl.symbol);
    std::string col = overrides.value("color", tpl.color);

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

    return id;
}

void InteractableFactory::AttachComponents(int id, const json& components, json overrides) {
    for (auto& [compName, compData] : components.items()) {
        if (compName == "portal") {
            PortalComponent portal;
            portal.destination_room = compData.value("destination_room", -1);
            portal.direction_command = compData.value("direction_command", "enter");
            portal.is_open = compData.value("is_open", true);
            portal.is_locked = compData.value("is_locked", false);
            portal.key_id = compData.value("key_id", -1);
            ctx.registry->AddComponent<PortalComponent>(id, portal);
        }
        else if (compName == "chest") {
            ChestComponent chest;
            chest.is_open = compData.value("is_open", false);
            chest.is_locked = compData.value("is_locked", false);
            chest.key_id = compData.value("key_id", -1);
            chest.loot_table = compData.value("loot_table", "");
            chest.max_uses = compData.value("max_uses", 1);
            chest.uses_remaining = chest.max_uses;
            ctx.registry->AddComponent<ChestComponent>(id, chest);
        }
        else if (compName == "door") {
            DoorComponent door;
            door.is_open = compData.value("is_open", false);
            door.is_locked = compData.value("is_locked", true);
            door.key_id = compData.value("key_id", "");
            door.destination_room = compData.value("destination_room", -1);
            door.destination_x = compData.value("destination_x", -1);
            door.destination_y = compData.value("destination_y", -1);
            door.open_message = compData.value("open_message", "The door creaks open.");
            door.close_message = compData.value("close_message", "The door slams shut.");
            ctx.registry->AddComponent<DoorComponent>(id, door);
        }
        else if (compName == "lever") {
            LeverComponent lever;
            lever.state = compData.value("state", "up");
            lever.event_trigger = compData.value("event_trigger", "");
            lever.target_room = compData.value("target_room", -1);
            lever.cooldown_seconds = compData.value("cooldown", 0);
            lever.can_be_reset = compData.value("can_be_reset", true);
            lever.uses_remaining = compData.value("uses_remaining", -1);
            ctx.registry->AddComponent<LeverComponent>(id, lever);
        }
        else if (compName == "healing") {
            ShrineComponent shrine;
            shrine.heal_amount = compData.value("heal_amount", 25);
            shrine.mana_amount = compData.value("mana_amount", 0);
            shrine.cooldown_seconds = compData.value("cooldown", 300);
            shrine.max_uses_per_player = compData.value("max_uses_per_player", -1);
            shrine.blessing_type = compData.value("blessing_type", "health");
            shrine.prayer_message = compData.value("prayer_message", "You feel blessed by divine energy.");
            ctx.registry->AddComponent<ShrineComponent>(id, shrine);
        }
        else if (compName == "inventory") {
            InteractableComponent interactable;
            interactable.interaction_type = "container";
            interactable.cooldown_seconds = compData.value("cooldown", 0);
            interactable.uses_remaining = compData.value("max_uses", -1);
            ctx.registry->AddComponent<InteractableComponent>(id, interactable);
        }
        else if (compName == "loot") {
            InteractableComponent interactable;
            interactable.interaction_type = "loot";
            interactable.custom_data["loot_table"] = compData.value("table", "");
            interactable.int_data["max_uses"] = compData.value("max_uses", 1);
            ctx.registry->AddComponent<InteractableComponent>(id, interactable);
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
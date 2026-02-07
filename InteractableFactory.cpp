#include "InteractableFactory.h"
#include "GameContext.h"
#include "ScriptManager.h"
#include <iostream>

void InteractableFactory::LoadSingleInteractableFromLua(std::string& key, sol::table& interactData) {
    // The script should return a table with the interactable data
    if (interactData.valid()) {
        InteractableTemplate t;
        t.name = interactData.get_or<std::string>("name", "Unknown Interactable");
        t.symbol = interactData.get_or<std::string>("char", "?");
        t.color = interactData.get_or<std::string>("color", "&w");
        t.description = interactData.get_or<std::string>("description", "");
        t.interactableType = interactData.get_or<std::string>("type", "generic");
        t.scriptPath = key;

        // Convert Lua table to JSON for components data
        sol::table components = interactData.get_or("components", sol::table());
        json componentsJson = json::object();

        // Iterate through the components table and convert to JSON
        for (auto& pair : components) {
            sol::object k = pair.first;
            sol::object value = pair.second;
            if (k.is<std::string>()) {
                std::string keyStr = k.as<std::string>();
                if (value.is<std::string>()) {
                    componentsJson[keyStr] = value.as<std::string>();
                } else if (value.is<int>()) {
                    componentsJson[keyStr] = value.as<int>();
                } else if (value.is<bool>()) {
                    componentsJson[keyStr] = value.as<bool>();
                } else if (value.is<double>()) {
                    componentsJson[keyStr] = value.as<double>();
                }
            }
        }

        t.extra = componentsJson;
        interactableTemplates[key] = t;

        std::cout << "Loaded interactable template: " << key << " (" << t.name << ")" << std::endl;
    } else {
        std::cerr << "[LUA ERROR] Interactable script did not return a data table: " << key << std::endl;
    }
}

InteractableTemplate InteractableFactory::GetTemplateDataWithOverrides(const std::string& templateID, json overrides) {
    auto it = interactableTemplates.find(templateID);
    if (it == interactableTemplates.end()) {
        return InteractableTemplate{}; // Return empty template
    }
    
    InteractableTemplate result = it->second;
    
    // Apply JSON overrides to template data
    if (overrides.contains("name")) {
        result.name = overrides["name"];
    }
    if (overrides.contains("description")) {
        result.description = overrides["description"];
    }
    if (overrides.contains("char")) {
        result.symbol = overrides["char"];
    }
    if (overrides.contains("color")) {
        result.color = overrides["color"];
    }
    
    // Merge component overrides
    if (overrides.contains("components")) {
        json components = overrides["components"];
        for (auto& [key, value] : components.items()) {
            result.extra[key] = value;
        }
    }
    
    return result;
}

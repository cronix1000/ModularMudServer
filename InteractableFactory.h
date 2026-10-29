#pragma once
#include "GameContext.h"
#include <fstream>
#include <iostream>
#include <map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "InteractableComponents.h"
#include "InventoryComponents.h"
#include "MovementComponents.h"
#include "PresentationComponents.h"
#include "WorldComponents.h"
#include "InteractableContext.h"
#include "Registry.h"
#include "ScriptManager.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct InteractableTemplate {
    std::string id;
    std::string name;
    std::string symbol;
    std::string color;
    std::string description;
    std::string script;
    json components;
};

class InteractableFactory {
public:
    GameContext& ctx;
    std::map<std::string, InteractableTemplate> interactableTemplates;

    InteractableFactory(GameContext& g) : ctx(g) {}

    void LoadInteractableTemplatesFromJSON(const std::string& path = "interactables.json");
    void LoadInteractableTemplatesFromJson(const json& data);

    int CreateInteractable(std::string templateID, json overrides = json::object(), int x = 0, int y = 0, int roomId = -1);

private:
    void LoadSingleInteractableFromJSON(const std::string& key, const json& data);
    void AttachComponents(int id, const json& components, json overrides);
};
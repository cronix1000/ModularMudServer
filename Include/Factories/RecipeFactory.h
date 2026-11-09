#pragma once
#include "GameContext.h"
#include <map>
#include <vector>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <utility>

using json = nlohmann::json;

struct RecipeInput {
    std::string templateId;
    int quantity = 1;
};

struct RecipeOutput {
    std::string templateId;
    int quantity = 1;
};

struct RecipeDef {
    std::string id;
    std::string name;
    std::string description;
    std::string skillId;
    int requiredSkillLevel = 0;
    std::string stationType;
    std::vector<RecipeInput> inputs;
    std::vector<RecipeOutput> outputs;
    float craftTimeSeconds = 3.0f;
    int experienceGain = 0;
    bool autoLearned = true;
};

class RecipeFactory {
public:
    GameContext& ctx;
    std::map<std::string, RecipeDef> recipes;
    std::map<std::string, std::string> skillIdToRecipeId;

    RecipeFactory(GameContext& g) : ctx(g) {}

    void LoadRecipesFromJson(const json& data);
    std::optional<RecipeDef> GetBySkill(const std::string& skillId) const;

private:
    void LoadSingleRecipeFromJson(const std::string& key, const json& data);
};
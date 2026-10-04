#include "RecipeFactory.h"
#include <iostream>

void RecipeFactory::LoadRecipesFromJson(const json& data) {
    std::cout << "[RecipeFactory] Loading recipes..." << std::endl;
    int count = 0;
    if (!data.is_object()) {
        std::cerr << "[RecipeFactory] Expected object, got non-object" << std::endl;
        return;
    }
    for (auto& [key, recipeData] : data.items()) {
        LoadSingleRecipeFromJson(key, recipeData);
        ++count;
    }
    std::cout << "[RecipeFactory] Loaded " << count << " recipes." << std::endl;
}

void RecipeFactory::LoadSingleRecipeFromJson(const std::string& key, const json& data) {
    RecipeDef r;
    r.id = key;
    r.name = data.value("name", key);
    r.description = data.value("description", "");
    r.skillId = data.value("skill_id", "");
    r.requiredSkillLevel = data.value("required_skill_level", 0);
    r.stationType = data.value("station_type", "");
    r.craftTimeSeconds = data.value("craft_time_seconds", 3.0f);
    r.experienceGain = data.value("experience_gain", 0);
    r.autoLearned = data.value("is_auto_learned", 1) != 0;

    if (data.contains("inputs") && data["inputs"].is_array()) {
        for (const auto& in : data["inputs"]) {
            RecipeInput ri;
            ri.templateId = in.value("template_id", in.value("item_id", ""));
            ri.quantity = in.value("quantity", 1);
            if (!ri.templateId.empty()) r.inputs.push_back(ri);
        }
    }

    if (data.contains("outputs") && data["outputs"].is_array()) {
        for (const auto& out : data["outputs"]) {
            RecipeOutput ro;
            ro.templateId = out.value("template_id", out.value("item_id", ""));
            ro.quantity = out.value("quantity", 1);
            if (!ro.templateId.empty()) r.outputs.push_back(ro);
        }
    }

    if (!r.skillId.empty()) {
        skillIdToRecipeId[r.skillId] = r.id;
    }
    recipes[r.id] = std::move(r);
}

std::optional<RecipeDef> RecipeFactory::GetBySkill(const std::string& skillId) const {
    auto it = skillIdToRecipeId.find(skillId);
    if (it == skillIdToRecipeId.end()) return std::nullopt;
    auto rit = recipes.find(it->second);
    if (rit == recipes.end()) return std::nullopt;
    return rit->second;
}
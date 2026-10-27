#include "CraftSystem.h"
#include <vector>
#include "RecipeFactory.h"
#include "Component.h"
#include "GameContext.h"
#include "Registry.h"
#include "ScriptManager.h"
#include "FactoryManager.h"

void CraftSystem::run() {

}


void CraftSystem::ProcessCraft(int sourceID, int skillID, float yieldMultiplier)
{
    auto* client = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (!client || !client->client) return;

    if (!ctx.factories) {
        client->client->QueueMessage("Crafting system unavailable.");
        return;
    }

    std::string skillKey = ctx.factories->skills.GetSkillKey(skillID);
    if (skillKey.empty()) {
        client->client->QueueMessage("That skill cannot be used for crafting.");
        return;
    }

    auto recipeOpt = ctx.factories->recipes.GetBySkill(skillKey);
    if (!recipeOpt) {
        client->client->QueueMessage("You don't know a recipe tied to that skill.");
        return;
    }
    const RecipeDef& recipe = *recipeOpt;

    auto* inventory = ctx.registry->GetComponent<InventoryComponent>(sourceID);
    if (!inventory) {
        client->client->QueueMessage("You have no inventory.");
        return;
    }

    std::vector<int> consumedIndices;
    for (const auto& in : recipe.inputs) {
        int found = 0;
        for (size_t i = 0; i < inventory->items.size() && found < in.quantity; ++i) {
            auto* itemComp = ctx.registry->GetComponent<ItemComponent>(inventory->items[i]);
            if (itemComp && itemComp->templateName == in.templateId) {
                consumedIndices.push_back(static_cast<int>(i));
                ++found;
            }
        }
        if (found < in.quantity) {
            client->client->QueueMessage("You lack the materials to craft " + recipe.name + ".");
            return;
        }
    }

    std::sort(consumedIndices.rbegin(), consumedIndices.rend());
    std::vector<int> consumedItems;
    for (int idx : consumedIndices) {
        consumedItems.push_back(inventory->items[idx]);
        inventory->items.erase(inventory->items.begin() + idx);
        ctx.registry->AddComponent<DestroyTag>(consumedItems.back(), DestroyTag{});
    }

    int totalYield = static_cast<int>(std::max<double>(1.0, std::floor(yieldMultiplier)));
    for (const auto& out : recipe.outputs) {
        int amount = out.quantity * totalYield;
        for (int i = 0; i < amount; ++i) {
            int newItem = ctx.factories->items.CreateItem(out.templateId, json::object());
            if (newItem > 0 && inventory->items.size() < static_cast<size_t>(inventory->max_slots)) {
                inventory->items.push_back(newItem);
            }
            else if (newItem > 0) {
                auto* pos = ctx.registry->GetComponent<PositionComponent>(sourceID);
                if (pos) {
                    ctx.registry->AddComponent<PositionComponent>(newItem, { pos->x, pos->y, pos->roomId });
                }
            }
        }
    }

    client->client->QueueMessage("You craft " + recipe.name + ".");
    ctx.registry->AddComponent<InventoryChangedComponent>(sourceID);

    if (recipe.experienceGain > 0 && ctx.scripts) {
        ctx.scripts->GrantExperience(sourceID, recipe.experienceGain, std::string("craft:") + recipe.id);
    }
}

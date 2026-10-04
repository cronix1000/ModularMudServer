#include "CombatSystem.h"
#include "GameContext.h"
#include "Registry.h"
#include "CombatIntentComponent.h"
#include "StatComponent.h"
#include "ClientComponent.h"
#include "BusyComponent.h"
#include "NameComponent.h"
#include "EventBus.h"
#include "MobComponent.h"
#include "Tags.h"
#include "InventoryComponent.h"
#include "FactoryManager.h"
#include "RecipeFactory.h"
#include "ItemFactory.h"
#include "ItemComponent.h"
#include "ScriptManager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <cmath>
#include <vector>

using json = nlohmann::json;

void CombatSystem::run()
{
    // Process all combat intents created by the skill system
    for (EntityID sourceID : ctx.registry->view<CombatIntentComponent>()) {
        auto* intent = ctx.registry->GetComponent<CombatIntentComponent>(sourceID);
        if (!intent) continue;

        //// Check if source is busy
        //auto* busy = ctx.registry->GetComponent<BusyComponent>(sourceID);
        //if (busy && busy->timeLeft > 0) {
        //    continue; 
        //}

        // Process the combat intent based on action type
        ProcessCombatIntent(sourceID, *intent);
        
        // Remove the intent after processing
        if(intent->attackOnce )
            ctx.registry->RemoveComponent<CombatIntentComponent>(sourceID);
    }
}

void CombatSystem::ProcessCombatIntent(int sourceID, const CombatIntentComponent& intent)
{
    // Route to appropriate handler based on action type
    if (intent.actionType == "attack") {
        // Check for critical hit
        bool isCritical = false;
        for (const auto& tag : intent.addedTags) {
            if (tag == "critical") {
                isCritical = true;
                break;
            }
        }
        ProcessAttack(sourceID, intent.targetID, intent.magnitude, intent.damageType, intent.dataString, isCritical);
    }
    else if (intent.actionType == "heal") {
        ProcessHeal(sourceID, intent.targetID, intent.magnitude);
    }
    else if (intent.actionType == "buff" || intent.actionType == "debuff") {
        ProcessBuff(sourceID, intent.targetID, intent.actionType, intent.magnitude);
    }
    else if (intent.actionType == "craft") {
        ProcessCraft(sourceID, intent.skillID, intent.magnitude);
    }
}

void CombatSystem::ProcessCraft(int sourceID, int skillID, float yieldMultiplier)
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
            } else if (newItem > 0) {
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

void CombatSystem::ProcessAttack(int sourceID, int targetID, float damage, const std::string& damageType,
                                 const std::string& attackVerb, bool isCritical)
{
    auto* targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
    if (!targetStats || targetStats->Health <= 0) return;

    auto* sourceStats = ctx.registry->GetComponent<StatComponent>(sourceID);
    if (!sourceStats) return;

    // Calculate final damage with armor
    int finalDamage = (std::max)(1, (int)(damage * sourceStats->AttackDamage) - targetStats->Armour);
    
    // Apply damage
    targetStats->Health = (std::max)(0, targetStats->Health - finalDamage);

    std::string critPrefix = isCritical ? "CRITICAL! " : "";
    std::string damageColor = isCritical ? "&y" : "&r"; // Yellow for crit, red for normal

    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient) {
        auto* targetName = ctx.registry->GetComponent<NameComponent>(targetID);
        std::string targetNameStr = targetName ? targetName->displayName : "target";
        
        json jsonData = {
            {"action", "attack"},
            {"damage", finalDamage},
            {"damage_type", damageType},
            {"target", targetNameStr},
            {"target_current_hp", targetStats->Health},
            {"target_max_hp", targetStats->MaxHealth},
            {"is_critical", isCritical}
        };
        
        GameMessage msg;
        msg.type = "combat_hit";
        msg.consoleText = critPrefix + "You " + attackVerb + " " + targetNameStr + " for " + damageColor + 
                         std::to_string(finalDamage) + "&w " + damageType + " damage!";
        msg.jsonData = jsonData.dump();
        sourceClient->QueueGameMessage(msg);
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient) {
        auto* sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
        std::string sourceNameStr = sourceName ? sourceName->displayName : "someone";
        
        json jsonData = {
            {"action", "attacked"},
            {"damage", finalDamage},
            {"damage_type", damageType},
            {"source", sourceNameStr},
            {"current_hp", targetStats->Health},
            {"max_hp", targetStats->MaxHealth},
            {"is_critical", isCritical}
        };
        
        GameMessage msg;
        msg.type = "combat_hit";
        msg.consoleText = critPrefix + sourceNameStr + " " + attackVerb + " you for " + damageColor + 
                         std::to_string(finalDamage) + "&w" + damageType + " damage!";
        msg.jsonData = jsonData.dump();
        targetClient->QueueGameMessage(msg);
        
        if (targetStats->Health <= 0) {
            json defeatData = {
                {"defeated_by", sourceNameStr},
                {"final_damage", finalDamage}
            };
            
            GameMessage defeatMsg;
            defeatMsg.type = "player_defeat";
            defeatMsg.consoleText = "&RYou have been defeated!&X";
            defeatMsg.jsonData = defeatData.dump();
            targetClient->QueueGameMessage(defeatMsg);
        }
    }


    // Handle mob death (separate from player death handling above)
    if (targetStats->Health <= 0) {
        auto* mobComp = ctx.registry->GetComponent<MobComponent>(targetID);
        if (mobComp) {
            // This is a mob that died - add DeadTag for RespawnSystem to handle
            ctx.registry->AddComponent<DeadTag>(targetID, DeadTag{});
        }
    }

    // Fire combat event for other systems
    CombatEventData ectx = { sourceID, targetID };
    EventContext data;
    data.data = ectx;
    ctx.eventBus->Publish(EventType::CombatHit, data);
}

void CombatSystem::ProcessHeal(int sourceID, int targetID, float healAmount)
{
    auto* targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
    if (!targetStats) return;

    int actualHeal = (std::min)((int)healAmount, targetStats->MaxHealth - targetStats->Health);
    targetStats->Health += actualHeal;

    // Send heal messages using new GameMessage pattern
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient && sourceID != targetID) {
        auto* targetName = ctx.registry->GetComponent<NameComponent>(targetID);
        std::string targetNameStr = targetName ? targetName->displayName : "target";
        
        json jsonData = {
            {"action", "heal"},
            {"heal_amount", actualHeal},
            {"target", targetNameStr},
            {"target_current_hp", targetStats->Health},
            {"target_max_hp", targetStats->MaxHealth}
        };
        
        GameMessage msg;
        msg.type = "combat_heal";
        msg.consoleText = "You heal " + targetNameStr + " for &G" + std::to_string(actualHeal) + "&X health!";
        msg.jsonData = jsonData.dump();
        sourceClient->QueueGameMessage(msg);
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient) {
        if (sourceID == targetID) {
            json jsonData = {
                {"action", "self_heal"},
                {"heal_amount", actualHeal},
                {"current_hp", targetStats->Health},
                {"max_hp", targetStats->MaxHealth}
            };
            
            GameMessage msg;
            msg.type = "combat_heal";
            msg.consoleText = "You heal yourself for &G" + std::to_string(actualHeal) + "&X health!";
            msg.jsonData = jsonData.dump();
            targetClient->QueueGameMessage(msg);
        } else {
            auto* sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
            std::string sourceNameStr = sourceName ? sourceName->displayName : "someone";
            
            json jsonData = {
                {"action", "healed"},
                {"heal_amount", actualHeal},
                {"source", sourceNameStr},
                {"current_hp", targetStats->Health},
                {"max_hp", targetStats->MaxHealth}
            };
            
            GameMessage msg;
            msg.type = "combat_heal";
            msg.consoleText = sourceNameStr + " heals you for &G" + std::to_string(actualHeal) + "&X health!";
            msg.jsonData = jsonData.dump();
            targetClient->QueueGameMessage(msg);
        }
    }
}

void CombatSystem::ProcessBuff(int sourceID, int targetID, const std::string& buffType, float magnitude)
{
    // For now, just send a message using new GameMessage pattern - buffs/debuffs would need a separate component system
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient) {
        json jsonData = {
            {"action", "cast_buff"},
            {"buff_type", buffType},
            {"magnitude", magnitude}
        };
        
        GameMessage msg;
        msg.type = "combat_buff";
        msg.consoleText = "You cast &C" + buffType + "&X on your target!";
        msg.jsonData = jsonData.dump();
        sourceClient->QueueGameMessage(msg);
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient) {
        json jsonData = {
            {"action", "buffed"},
            {"buff_type", buffType},
            {"magnitude", magnitude}
        };
        
        GameMessage msg;
        msg.type = "combat_buff";
        msg.consoleText = "You are affected by &C" + buffType + "&X!";
        msg.jsonData = jsonData.dump();
        targetClient->QueueGameMessage(msg);
    }
    
    // TODO: Implement actual buff/debuff system with temporary stat modifiers
}

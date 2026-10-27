#include "InventorySystem.h"
#include "GameContext.h"
#include "Registry.h"
#include "Component.h"
#include "EquipmentSlot.h"
#include "EquipmentSlotInfo.h"
#include "ScriptComponent.h"
#include "ItemScriptContext.h"
#include "ItemScriptResult.h"
#include "ScriptManager.h"
#include "ClientComponent.h"
#include "ClientConnection.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include "EventBus.h"
#include "FactoryManager.h"

void ApplyStatBonus(StatComponent* stats, const std::string& key, int val) {
    if (key == "strength") stats->Strength += val;
    else if (key == "armour") stats->Armour += val;
    else if (key == "health" || key == "hp") stats->MaxHealth += val;
    else if (key == "mana") stats->Mana += val;
    else if (key == "magic_defense") stats->MagicDefense += val;
}

void InventorySystem::Run(float dt) {
    // Use the new, efficient view to iterate over entities wanting to equip an item.
    for (EntityID entity : ctx.registry->view<EquipItemIntentComponent>()) {
        auto* intent = ctx.registry->GetComponent<EquipItemIntentComponent>(entity);
        if (!intent) continue;

        InventoryComponent* inventory = ctx.registry->GetComponent<InventoryComponent>(entity);
        EquipmentComponent* equipment = ctx.registry->GetComponent<EquipmentComponent>(entity);

        // Ensure the entity has the necessary components to perform this action.
        if (!inventory || !equipment) {
            ctx.registry->RemoveComponent<EquipItemIntentComponent>(entity);
            continue;
        }

        EquipmentSlot finalSlot = EquipmentSlot::Default;
        bool didEquip = false;

        auto* weapon = ctx.registry->GetComponent<WeaponComponent>(intent->itemId);
        if (weapon) {
            EquipmentSlot slot = intent->slot;
            if (slot == EquipmentSlot::Default) {
                slot = EquipmentSlot::MainArm;
            }

            if (weapon->twoHanded && slot == EquipmentSlot::MainArm &&
                equipment->slots.count(EquipmentSlot::OffHand) &&
                equipment->slots[EquipmentSlot::OffHand] > 0) {
                inventory->items.push_back(equipment->slots[EquipmentSlot::OffHand]);
                equipment->slots.erase(EquipmentSlot::OffHand);
            }

            // Unequip old item, if any.
            if (equipment->slots.count(slot) && equipment->slots[slot] > 0) {
                inventory->items.push_back(equipment->slots[slot]);
            }

            equipment->slots[slot] = intent->itemId;
            finalSlot = slot;
            didEquip = true;

            // Update the default "attack" skill alias.
            auto* itemComp = ctx.registry->GetComponent<ItemComponent>(intent->itemId);
            auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(entity);
            if (skillHolder && itemComp) {
                // 1. Update the generic "attack" command to use the item's primary skill
                if (itemComp->primarySkillId != -1) {
                    skillHolder->skillAliases["attack"] = itemComp->primarySkillId;
                }

                // 2. Map the extra skills by their actual names so the player can call them
                for (int sId : itemComp->extraSkillIds) {
                    auto* nameComp = ctx.registry->GetComponent<NameComponent>(sId);
                    if (nameComp) {
                        // e.g. skillAliases["Heavy Slam"] = 402;
                        skillHolder->skillAliases[nameComp->displayName] = sId;
                    }
                }
            }
        }

        auto* armour = ctx.registry->GetComponent<ArmourComponent>(intent->itemId);
        if (armour) {
            auto* itemC = ctx.registry->GetComponent<ItemComponent>(intent->itemId);
            EquipmentSlot slot = EquipmentSlot::Default;
            if (itemC && !itemC->allowedSlots.empty()) {
                bool intentValid = std::find(itemC->allowedSlots.begin(), itemC->allowedSlots.end(), intent->slot) != itemC->allowedSlots.end();
                if (intent->slot != EquipmentSlot::Default && intentValid) {
                    slot = intent->slot;
                } else {
                    slot = EquipmentSlotInfo::PickFirstAllowedEmpty(*equipment, itemC->allowedSlots);
                    if (slot == EquipmentSlot::Default) {
                        slot = itemC->allowedSlots.front();
                    }
                }
            }
            if (slot == EquipmentSlot::Default) {
                slot = EquipmentSlot::Chest;
            }
            if (equipment->slots.count(slot) && equipment->slots[slot] > 0) {
                inventory->items.push_back(equipment->slots[slot]);
            }
            equipment->slots[slot] = intent->itemId;
            finalSlot = slot;
            didEquip = true;
        }

        if (!didEquip) {
            ctx.registry->RemoveComponent<EquipItemIntentComponent>(entity);
            continue;
        }

        // Lua hook: per-item on_equip script
        bool cancel = false;
        if (ctx.scripts) {
            auto* scriptComp = ctx.registry->GetComponent<ScriptComponent>(intent->itemId);
            if (scriptComp) {
                auto it = scriptComp->scripts_path.find("on_equip");
                if (it != scriptComp->scripts_path.end() && !it->second.empty()) {
                    ItemScriptContext isctx;
                    isctx.userID = entity;
                    isctx.itemID = intent->itemId;
                    isctx.slot = finalSlot;
                    auto* itemC = ctx.registry->GetComponent<ItemComponent>(intent->itemId);
                    if (itemC) isctx.itemTemplateID = itemC->templateName;
                    auto* playerPos = ctx.registry->GetComponent<PositionComponent>(entity);
                    if (playerPos) {
                        isctx.roomID = playerPos->roomId;
                        isctx.x = playerPos->x;
                        isctx.y = playerPos->y;
                    }
                    ItemScriptResult res = ctx.scripts->ExecuteItemScript(it->second, "on_execute", isctx);
                    if (!res.message.empty()) {
                        auto* clientComp = ctx.registry->GetComponent<ClientComponent>(entity);
                        if (clientComp && clientComp->client) {
                            clientComp->client->QueueMessage(res.message);
                        }
                    }
                    if (res.cancelEquip) {
                        cancel = true;
                    }
                }
            }
        }

        if (cancel) {
            // Revert: free the slot; the item never left the inventory list.
            equipment->slots.erase(finalSlot);
            if (std::find(inventory->items.begin(), inventory->items.end(), intent->itemId) == inventory->items.end()) {
                inventory->items.push_back(intent->itemId);
            }
            ctx.registry->RemoveComponent<EquipItemIntentComponent>(entity);
            continue;
        }

        auto itemIt = std::find(inventory->items.begin(), inventory->items.end(), intent->itemId);
        if (itemIt != inventory->items.end()) {
            inventory->items.erase(itemIt);
        }

        // Publish event and mark inventory as changed.
        EventContext data;
        data.data = ItemEquippedEventData{ entity, intent->itemId, finalSlot };
        ctx.eventBus->Publish(EventType::ItemEquipped, data);
        ctx.registry->AddComponent<InventoryChangedComponent>(entity);

        RecalculateStats(entity);

        // Remove the intent component after it has been fully processed.
        ctx.registry->RemoveComponent<EquipItemIntentComponent>(entity);
    }

    for (EntityID entity : ctx.registry->view<UnequipItemIntentComponent>()) {
        auto* intent = ctx.registry->GetComponent<UnequipItemIntentComponent>(entity);
        if (!intent) {
            ctx.registry->RemoveComponent<UnequipItemIntentComponent>(entity);
            continue;
        }

        InventoryComponent* inventory = ctx.registry->GetComponent<InventoryComponent>(entity);
        EquipmentComponent* equipment = ctx.registry->GetComponent<EquipmentComponent>(entity);

        if (!inventory || !equipment) {
            ctx.registry->RemoveComponent<UnequipItemIntentComponent>(entity);
            continue;
        }

        int itemID = intent->itemId;
        EquipmentSlot targetSlot = intent->slot;

        if (targetSlot == EquipmentSlot::Default) {
            targetSlot = EquipmentSlotInfo::FindSlotOfEntity(*equipment, itemID);
        }

        bool removed = false;
        if (targetSlot != EquipmentSlot::Default &&
            equipment->slots.count(targetSlot) &&
            equipment->slots[targetSlot] == itemID) {
            equipment->slots.erase(targetSlot);
            inventory->items.push_back(itemID);
            removed = true;
        } else {
            for (auto it = equipment->slots.begin(); it != equipment->slots.end(); ++it) {
                if (it->second == itemID) {
                    targetSlot = it->first;
                    equipment->slots.erase(it);
                    inventory->items.push_back(itemID);
                    removed = true;
                    break;
                }
            }
        }

        if (removed) {
            auto* itemComp = ctx.registry->GetComponent<ItemComponent>(itemID);
            auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(entity);

            if (skillHolder && itemComp) {
                if (itemComp->primarySkillId != -1) {
                    auto aliasIt = skillHolder->skillAliases.find("attack");
                    if (aliasIt != skillHolder->skillAliases.end() &&
                        aliasIt->second == itemComp->primarySkillId) {
                        skillHolder->skillAliases.erase(aliasIt);
                    }
                }
                for (int sId : itemComp->extraSkillIds) {
                    auto* nameComp = ctx.registry->GetComponent<NameComponent>(sId);
                    if (!nameComp) continue;
                    auto aliasIt = skillHolder->skillAliases.find(nameComp->displayName);
                    if (aliasIt != skillHolder->skillAliases.end() &&
                        aliasIt->second == sId) {
                        skillHolder->skillAliases.erase(aliasIt);
                    }
                }
            }

            // Lua hook: per-item on_unequip script (cancellable).
            bool cancel = false;
            if (ctx.scripts) {
                auto* scriptComp = ctx.registry->GetComponent<ScriptComponent>(itemID);
                if (scriptComp) {
                    auto it = scriptComp->scripts_path.find("on_unequip");
                    if (it != scriptComp->scripts_path.end() && !it->second.empty()) {
                        ItemScriptContext isctx;
                        isctx.userID = entity;
                        isctx.itemID = itemID;
                        isctx.slot = targetSlot;
                        if (itemComp) isctx.itemTemplateID = itemComp->templateName;
                        auto* playerPos = ctx.registry->GetComponent<PositionComponent>(entity);
                        if (playerPos) {
                            isctx.roomID = playerPos->roomId;
                            isctx.x = playerPos->x;
                            isctx.y = playerPos->y;
                        }
                        ItemScriptResult res = ctx.scripts->ExecuteItemScript(it->second, "on_execute", isctx);
                        if (!res.message.empty()) {
                            auto* clientComp = ctx.registry->GetComponent<ClientComponent>(entity);
                            if (clientComp && clientComp->client) {
                                clientComp->client->QueueMessage(res.message);
                            }
                        }
                        if (res.cancelUnequip) cancel = true;
                    }
                }
            }

            if (cancel) {
                // Revert: restore the slot, take the item back out of inventory.
                equipment->slots[targetSlot] = itemID;
                auto it2 = std::find(inventory->items.begin(), inventory->items.end(), itemID);
                if (it2 != inventory->items.end()) inventory->items.erase(it2);
            } else {
                EventContext evt;
                evt.data = ItemUnequippedEventData{ entity, itemID, targetSlot };
                ctx.eventBus->Publish(EventType::ItemUnequipped, evt);
                ctx.registry->AddComponent<InventoryChangedComponent>(entity);

                RecalculateStats(entity);
            }
        }

        ctx.registry->RemoveComponent<UnequipItemIntentComponent>(entity);
    }
}

void InventorySystem::RecalculateStats(int entityID)
{
    InventoryComponent* inventory = ctx.registry->GetComponent<InventoryComponent>(entityID);
    EquipmentComponent* equipment = ctx.registry->GetComponent<EquipmentComponent>(entityID);
    BaseStatComponent* baseStats = ctx.registry->GetComponent<BaseStatComponent>(entityID);
    StatComponent* stats = ctx.registry->GetComponent<StatComponent>(entityID);

    if (!stats || !baseStats || !equipment) return;  // Add safety check

    // Reset to base stats
    stats->Armour = baseStats->Armour;
    stats->AttackDamage = baseStats->AttackDamage;
    stats->DamageType = baseStats->damageType;
    stats->MagicDamage = baseStats->MagicDamage;
    stats->MagicDefense = baseStats->MagicDefense;
    stats->Mana = baseStats->Mana;
    stats->MaxHealth = baseStats->MaxHealth;
    stats->Strength = baseStats->Strength;

    for (auto const& [slotName, itemID] : equipment->slots) {
        if (itemID <= 0) continue;  // Skip empty slots

        // Check if the item has Armour
        auto* armour = ctx.registry->GetComponent<ArmourComponent>(itemID);
        if (armour) {
            stats->Armour += armour->defense;
            stats->MagicDefense += armour->magicDefense;
        }

        auto* item = ctx.registry->GetComponent<ItemComponent>(itemID);

        // loop through sockets when implemented
        // check if the item has synergy bonuses
        // #TODO

        auto* mods = ctx.registry->GetComponent<StatModifierComponent>(itemID);
        if (mods) {
            for (auto const& [statName, value] : mods->modifiers) {
                ApplyStatBonus(stats, statName, value);
            }
        }

        if (slotName == EquipmentSlot::MainArm) {
            auto* weapon = ctx.registry->GetComponent<WeaponComponent>(itemID);
            if (weapon) {
                // 1. Calculate Attribute Scaling
                int bonus = 0;
                bonus += (int)(stats->Strength * weapon->strScaling);
                bonus += (int)(stats->Dexterity * weapon->dexScaling);
                bonus += (int)(stats->Intelligence * weapon->intScaling);

                // 3. Add to the Player's "Flat Bonus"
                stats->AttackDamage += bonus;
            }
        }
    }
}
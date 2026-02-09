#include "InventorySystem.h"
#include "GameContext.h"     
#include "Registry.h"      
#include "Component.h"       
#include "EquipmentSlot.h"
#include <vector>             
#include <iostream>         
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

        auto* weapon = ctx.registry->GetComponent<WeaponComponent>(intent->itemId);
        if (weapon) {
            EquipmentSlot slot = intent->slot;
            if (slot == EquipmentSlot::Default) { // Use corrected enum name
                slot = EquipmentSlot::mainArm;
            }

            // Unequip old item, if any.
            if (equipment->slots.count(slot) && equipment->slots[slot] > 0) {
                inventory->items.push_back(equipment->slots[slot]);
            }

            equipment->slots[slot] = intent->itemId;

            // Update the default "attack" skill alias.
            auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(entity);
            if (skillHolder) {
                int skillID = ctx.factories->skills.GetSkillID(weapon->defaultSkillTemplate);
                skillHolder->skillAliases["attack"] = skillID;
            }
        }

        auto* armour = ctx.registry->GetComponent<ArmourComponent>(intent->itemId);
        if (armour) {
            EquipmentSlot slot = armour->slot;
            if (equipment->slots.count(slot) && equipment->slots[slot] > 0) {
                inventory->items.push_back(equipment->slots[slot]);
            }
            equipment->slots[slot] = intent->itemId;
        }

        // Publish event and mark inventory as changed.
        EventContext data;
        data.data = ItemEquippedEventData{ entity, intent->itemId };
        ctx.eventBus->Publish(EventType::ItemEquipped, data);
        ctx.registry->AddComponent<InventoryChangedComponent>(entity);

        RecalculateStats(entity);
        
        // Remove the intent component after it has been fully processed.
        ctx.registry->RemoveComponent<EquipItemIntentComponent>(entity);
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

        if (slotName == EquipmentSlot::mainArm) {
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
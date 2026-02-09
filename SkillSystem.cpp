#include "SkillSystem.h"
#include "Component.h"
#include "Registry.h"
#include "GameContext.h"
#include "TimeData.h"
#include "SkillContext.h"
#include "ScriptManager.h"
#include "CombatIntentComponent.h"


void SkillSystem::Run(float dt)
{
    // Process all entities with a skill windup.
    // The view provides a list of entities, which is then used to get the component.
    for (EntityID entity : ctx.registry->view<SkillWindupComponent>()) {
        auto* windup = ctx.registry->GetComponent<SkillWindupComponent>(entity);
        if (!windup) continue;

        windup->timeLeft -= dt;

        if (windup->timeLeft <= 0) {
            // Windup finished, execute the skill.
            ExecuteScriptAndDispatch(entity, windup->skillID, windup->targetID);
            // Remove the component immediately after processing.
            ctx.registry->RemoveComponent<SkillWindupComponent>(entity);
        }
    }

    // Process all entities with a skill intent.
    for (EntityID entity : ctx.registry->view<SkillIntentComponent>()) {
        auto* intent = ctx.registry->GetComponent<SkillIntentComponent>(entity);
        if (!intent) continue;

        auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(entity);
        // Using intent->skillId instead of intent.skillId because intent is a pointer
        auto* cooldown = ctx.registry->GetComponent<CooldownStatsComponent>(intent->skillId);

        if (!skillHolder || !cooldown) {
            ctx.registry->RemoveComponent<SkillIntentComponent>(entity);
            continue;
        }

        if (!skillHolder->IsReady(intent->skillId, ctx.time->globalTime)) {
            // Skill is not ready (on cooldown).
            // Optionally send a message to the player.
            // We still remove the intent as it has been "handled" (by being rejected).
        } else {
            float windupTime = cooldown->windupTime;

            if (windupTime > 0.0f) {
                // Skill has a windup time, so add the windup component.
                // The loop above will handle it in subsequent frames.
                ctx.registry->AddComponent<SkillWindupComponent>(entity, {
                    windupTime, intent->skillId, intent->targetId
                });
            } else {
                // Instant cast skill.
                ExecuteScriptAndDispatch(entity, intent->skillId, intent->targetId);
            }
            // Set the skill on cooldown *after* it has been successfully initiated.
            skillHolder->SetCooldown(intent->skillId, ctx.time->globalTime, cooldown->cooldownTime);
        }
        
        // Remove the intent component now that it has been handled.
        ctx.registry->RemoveComponent<SkillIntentComponent>(entity);
    }
}


void SkillSystem::ExecuteScriptAndDispatch(int entityID, int skillID, int targetID) {
        auto* scriptComp = ctx.registry->GetComponent<ScriptComponent>(skillID);

        if (!scriptComp) {
            std::cerr << "[Error] Skill ID " << skillID << " has no ScriptComponent!" << std::endl;
            return;
        }

        // 2. FIND THE SPECIFIC SCRIPT ("on_use")
        // Since it's a map, we must look for the specific trigger key.
        // We assume "on_use" is the standard key for casting/activating a skill.
        std::string scriptKey = "on_use";

        if (scriptComp->scripts_path.find(scriptKey) == scriptComp->scripts_path.end()) {
            std::cerr << "[Error] Skill ID " << skillID << " has no script registered for '" << scriptKey << "'!" << std::endl;
            return;
        }

        std::string path = scriptComp->scripts_path.at(scriptKey);

        // 3. CALCULATE BASE POWER (Weapon Damage or Tool Tier)
        int power = 0;
        auto* equipment = ctx.registry->GetComponent<EquipmentComponent>(entityID);
        if (equipment) {
            int weaponID = equipment->slots[EquipmentSlot::mainArm];
            auto* weapon = ctx.registry->GetComponent<WeaponComponent>(weaponID);
            if (weapon) {
                power = weapon->maxDamage;
            }
        }

        // 4. PREPARE CONTEXT
        SkillContext context;
        context.sourceID = entityID;
        context.targetID = targetID;
        context.skillID = skillID;
        context.basePower = power;

        auto* holder = ctx.registry->GetComponent<SkillHolderComponent>(entityID);
        context.masteryLevel = holder ? holder->GetMastery(skillID) : 1;

        // 5. RUN LUA SCRIPT
        SkillResult result = ctx.scripts->ExecuteSkillScript(path, context);

        // 6. DISPATCH RESULT
        if (!result.success) return;

        // Pass the skillID through the result for proper intent creation
        result.skillID = skillID;
        ProcessResult(entityID, result, targetID);
}

void SkillSystem::ProcessResult(int entity, SkillResult result, int targetId)
{
    // Create a comprehensive combat intent from the skill result
    CombatIntentComponent combatIntent;
    combatIntent.sourceID = entity;
    combatIntent.targetID = targetId;
    combatIntent.skillID = result.skillID; // We'll need to pass this through
    combatIntent.actionType = result.actionType;
    combatIntent.magnitude = result.magnitude;
    combatIntent.damageType = result.damageType;
    combatIntent.dataString = result.dataString;
    combatIntent.addedTags = result.addedTags;
    
    // Add the combat intent to the source entity
    ctx.registry->AddComponent<CombatIntentComponent>(entity, combatIntent);
}
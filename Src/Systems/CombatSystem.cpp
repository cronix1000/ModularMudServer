#include "CombatSystem.h"
#include "Component.h"
#include "EventBus.h"
#include "GameContext.h"
#include "MobAttackBuilder.h"
#include "MobAttackComponent.h"
#include "PowerCalculator.h"
#include "Registry.h"
#include "SkillXP.h"
#include "TimeData.h"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {
std::mt19937 &Rng() {
  static std::mt19937 rng(static_cast<unsigned>(time(nullptr)));
  return rng;
}
} // namespace

void CombatSystem::run() {
  std::vector<EntityID> removeComp;
  double now = ctx.time->globalTime;

  for (EntityID sourceID : ctx.registry->view<CombatIntentComponent>()) {
    auto *intent = ctx.registry->GetComponent<CombatIntentComponent>(sourceID);
    if (!intent)
      continue;

    std::vector<CombatIntentComponent> queued =
        std::move(intent->queuedOneShots);
    intent->queuedOneShots.clear();
    for (auto &oneShot : queued) {
      ProcessCombatIntent(sourceID, oneShot);
    }

    if (intent->nextFireTime > now)
      continue;

    if (!intent->attackOnce && intent->actionType == "attack") {
      MobAttackRoll roll = MobAttackBuilder::BuildAttack(
          ctx.registry.get(), sourceID, intent->targetID);
      if (roll.valid) {
        intent->magnitude = roll.multiplier;
        intent->dataString = roll.verb;
        intent->damageType = roll.damageType;
      }
    }

    ProcessCombatIntent(sourceID, *intent);

    if (ShouldRenew(*intent)) {
      intent->nextFireTime =
          now + PowerCalculator::AttackInterval(ctx.registry.get(), sourceID);
    } else {
      removeComp.push_back(sourceID);
    }
  }
  for (EntityID id : removeComp) {
    ctx.registry->RemoveComponent<CombatIntentComponent>(id);
  }
}

bool CombatSystem::ShouldRenew(const CombatIntentComponent &intent) {
  if (intent.attackOnce || intent.actionType != "attack")
    return false;
  if (intent.targetID <= 0)
    return false;

  auto *targetStats =
      ctx.registry->GetComponent<StatComponent>(intent.targetID);
  if (!targetStats || targetStats->Health <= 0)
    return false;

  auto *sourcePos =
      ctx.registry->GetComponent<PositionComponent>(intent.sourceID);
  auto *targetPos =
      ctx.registry->GetComponent<PositionComponent>(intent.targetID);
  if (!sourcePos || !targetPos || sourcePos->roomId != targetPos->roomId)
    return false;

  return true;
}

float CombatSystem::RollCritChance(int sourceID) {
  auto *stats = ctx.registry->GetComponent<StatComponent>(sourceID);
  if (!stats)
    return 0.0f;

  float base = 0.05f;
  auto *mobAttack = ctx.registry->GetComponent<MobAttackComponent>(sourceID);
  if (mobAttack)
    base = mobAttack->criticalChance;

  return (std::min)(0.25f, base + stats->Dexterity * 0.001f);
}

bool CombatSystem::IsMagicDamage(const std::string &damageType) {
  static const std::vector<std::string> magicTypes = {
      "magic", "fire", "ice", "frost", "lightning", "arcane"};
  return std::find(magicTypes.begin(), magicTypes.end(), damageType) !=
         magicTypes.end();
}

void CombatSystem::ProcessCombatIntent(int sourceID,
                                       const CombatIntentComponent &intent) {
  // Route to appropriate handler based on action type
  if (intent.actionType == "attack") {
    bool forcedCritical = false;
    for (const auto &tag : intent.addedTags) {
      if (tag == "critical") {
        forcedCritical = true;
        break;
      }
    }
    ProcessAttack(sourceID, intent.targetID, intent.magnitude,
                  intent.damageType, intent.dataString, forcedCritical,
                  intent.skillID);
  } else if (intent.actionType == "heal") {
    ProcessHeal(sourceID, intent.targetID, intent.magnitude);
  } else if (intent.actionType == "buff" || intent.actionType == "debuff") {
    ProcessBuff(sourceID, intent.targetID, intent.actionType, intent.magnitude);
  }
}

void CombatSystem::ProcessAttack(int sourceID, int targetID, float multiplier,
                                 const std::string &damageType,
                                 const std::string &attackVerb,
                                 bool forceCritical, int skillID) {
  auto *targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
  if (!targetStats || targetStats->Health <= 0)
    return;

  auto *sourceStats = ctx.registry->GetComponent<StatComponent>(sourceID);
  if (!sourceStats)
    return;

  bool magic = IsMagicDamage(damageType);
  int base = magic ? sourceStats->MagicPower : sourceStats->Power;
  int mitigation = magic ? targetStats->MagicDefense : targetStats->Armour;

  bool isCritical = forceCritical;
  if (!isCritical) {
    std::uniform_real_distribution<float> critDist(0.0f, 1.0f);
    isCritical = critDist(Rng()) < RollCritChance(sourceID);
  }

  std::uniform_real_distribution<float> varianceDist(0.9f, 1.1f);
  float raw = base * multiplier * varianceDist(Rng());
  if (isCritical)
    raw *= 1.5f;

  int finalDamage =
      (std::max)(1, static_cast<int>(std::lround(raw)) - mitigation);

  // Apply damage
  targetStats->Health = (std::max)(0, targetStats->Health - finalDamage);

  std::string critPrefix = isCritical ? "CRITICAL! " : "";
  std::string damageColor =
      isCritical ? "&y" : "&r"; // Yellow for crit, red for normal

  auto *sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
  if (sourceClient) {
    auto *targetName = ctx.registry->GetComponent<NameComponent>(targetID);
    std::string targetNameStr = targetName ? targetName->displayName : "target";

    json jsonData = {{"action", "attack"},
                     {"damage", finalDamage},
                     {"damage_type", damageType},
                     {"target", targetNameStr},
                     {"target_current_hp", targetStats->Health},
                     {"target_max_hp", targetStats->MaxHealth},
                     {"is_critical", isCritical}};

    GameMessage msg;
    msg.type = "combat_hit";
    msg.consoleText = critPrefix + "You " + attackVerb + " " + targetNameStr +
                      " for " + damageColor + std::to_string(finalDamage) +
                      "&w " + damageType + " damage!";
    msg.jsonData = jsonData.dump();
    sourceClient->QueueGameMessage(msg);
  }

  auto *targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
  if (targetClient) {
    auto *sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
    std::string sourceNameStr =
        sourceName ? sourceName->displayName : "someone";

    json jsonData = {{"action", "attacked"},
                     {"damage", finalDamage},
                     {"damage_type", damageType},
                     {"source", sourceNameStr},
                     {"current_hp", targetStats->Health},
                     {"max_hp", targetStats->MaxHealth},
                     {"is_critical", isCritical}};

    GameMessage msg;
    msg.type = "combat_hit";
    msg.consoleText = critPrefix + sourceNameStr + " " + attackVerb +
                      " you for " + damageColor + std::to_string(finalDamage) +
                      "&w" + damageType + " damage!";
    msg.jsonData = jsonData.dump();
    targetClient->QueueGameMessage(msg);
  }

  if (targetStats->Health <= 0) {
    HandleDeath(sourceID, targetID, skillID, finalDamage);
  }

  // Fire combat event for other systems
  CombatEventData ectx = {sourceID, targetID};
  EventContext data;
  data.data = ectx;
  ctx.eventBus->Publish(EventType::CombatHit, data);
}

void CombatSystem::HandleDeath(int sourceID, int targetID, int skillID,
                               int finalDamage) {
  auto *targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
  if (targetClient) {
    auto *sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
    std::string sourceNameStr =
        sourceName ? sourceName->displayName : "someone";

    json defeatData = {{"defeated_by", sourceNameStr},
                       {"final_damage", finalDamage}};

    GameMessage defeatMsg;
    defeatMsg.type = "player_defeat";
    defeatMsg.consoleText = "&RYou have been defeated!&X";
    defeatMsg.jsonData = defeatData.dump();
    targetClient->QueueGameMessage(defeatMsg);
  }

  auto *mobComp = ctx.registry->GetComponent<MobComponent>(targetID);
  if (mobComp) {
    // This is a mob that died - add DeadTag for RespawnSystem to handle
    ctx.registry->AddComponent<DeadTag>(targetID, DeadTag{});

    auto *sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient) {
      auto *targetName = ctx.registry->GetComponent<NameComponent>(targetID);
      std::string targetNameStr =
          targetName ? targetName->displayName : "target";

      json killData = {{"victim", targetNameStr}, {"skill_id", skillID}};

      GameMessage killMsg;
      killMsg.type = "mob_slain";
      killMsg.consoleText = "&YYou have slain " + targetNameStr + "!&X";
      killMsg.jsonData = killData.dump();
      sourceClient->QueueGameMessage(killMsg);
    }

    SkillXP::GrantKillBonus(ctx, sourceID, skillID);
  }

  EntityDeathEventData eventData = {sourceID, targetID, skillID};
  EventContext eventContext;
  eventContext.data = eventData;
  ctx.eventBus->Publish(EventType::EntityDeath, eventContext);
}

void CombatSystem::ProcessHeal(int sourceID, int targetID, float healAmount) {
  auto *targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
  if (!targetStats)
    return;

  int actualHeal =
      (std::min)((int)healAmount, targetStats->MaxHealth - targetStats->Health);
  targetStats->Health += actualHeal;

  // Send heal messages using new GameMessage pattern
  auto *sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
  if (sourceClient && sourceID != targetID) {
    auto *targetName = ctx.registry->GetComponent<NameComponent>(targetID);
    std::string targetNameStr = targetName ? targetName->displayName : "target";

    json jsonData = {{"action", "heal"},
                     {"heal_amount", actualHeal},
                     {"target", targetNameStr},
                     {"target_current_hp", targetStats->Health},
                     {"target_max_hp", targetStats->MaxHealth}};

    GameMessage msg;
    msg.type = "combat_heal";
    msg.consoleText = "You heal " + targetNameStr + " for &G" +
                      std::to_string(actualHeal) + "&X health!";
    msg.jsonData = jsonData.dump();
    sourceClient->QueueGameMessage(msg);
  }

  auto *targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
  if (targetClient) {
    if (sourceID == targetID) {
      json jsonData = {{"action", "self_heal"},
                       {"heal_amount", actualHeal},
                       {"current_hp", targetStats->Health},
                       {"max_hp", targetStats->MaxHealth}};

      GameMessage msg;
      msg.type = "combat_heal";
      msg.consoleText = "You heal yourself for &G" +
                        std::to_string(actualHeal) + "&X health!";
      msg.jsonData = jsonData.dump();
      targetClient->QueueGameMessage(msg);
    } else {
      auto *sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
      std::string sourceNameStr =
          sourceName ? sourceName->displayName : "someone";

      json jsonData = {{"action", "healed"},
                       {"heal_amount", actualHeal},
                       {"source", sourceNameStr},
                       {"current_hp", targetStats->Health},
                       {"max_hp", targetStats->MaxHealth}};

      GameMessage msg;
      msg.type = "combat_heal";
      msg.consoleText = sourceNameStr + " heals you for &G" +
                        std::to_string(actualHeal) + "&X health!";
      msg.jsonData = jsonData.dump();
      targetClient->QueueGameMessage(msg);
    }
  }
}

void CombatSystem::ProcessBuff(int sourceID, int targetID,
                               const std::string &buffType, float magnitude) {
  // For now, just send a message using new GameMessage pattern - buffs/debuffs
  // would need a separate component system
  auto *sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
  if (sourceClient) {
    json jsonData = {{"action", "cast_buff"},
                     {"buff_type", buffType},
                     {"magnitude", magnitude}};

    GameMessage msg;
    msg.type = "combat_buff";
    msg.consoleText = "You cast &C" + buffType + "&X on your target!";
    msg.jsonData = jsonData.dump();
    sourceClient->QueueGameMessage(msg);
  }

  auto *targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
  if (targetClient) {
    json jsonData = {{"action", "buffed"},
                     {"buff_type", buffType},
                     {"magnitude", magnitude}};

    GameMessage msg;
    msg.type = "combat_buff";
    msg.consoleText = "You are affected by &C" + buffType + "&X!";
    msg.jsonData = jsonData.dump();
    targetClient->QueueGameMessage(msg);
  }

  // TODO: Implement actual buff/debuff system with temporary stat modifiers
}

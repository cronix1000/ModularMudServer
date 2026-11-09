#include "SkillXP.h"
#include "Component.h"
#include "EventBus.h"
#include "FactoryManager.h"
#include "GameContext.h"
#include "PowerCalculator.h"
#include "Registry.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
constexpr int XP_KILL_BONUS = 50;
}

namespace PlayerXP {
void Grant(GameContext &ctx, int playerID, int amount) {
  // grant less based on each enemy kill max xp per first kill
  // Kills give variable xp based on mob itself
  // e.g. Boss might give 100 000 xp first time but 95 000 second time
  auto *XPHolder = ctx.registry->GetComponent<LevelComponent>(playerID);
}
} // namespace PlayerXP

namespace SkillXP {

void Grant(GameContext &ctx, int playerID, int skillID, int amount) {
  auto *holder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);
  if (!holder)
    return;

  bool leveledUp = holder->AddXP(skillID, amount);
  if (leveledUp) {

    auto *skillDef =
        ctx.registry->GetComponent<SkillDefinitionComponent>(skillID);
    std::string skillName = skillDef ? skillDef->name : "skill";
    int newLevel = holder->GetMastery(skillID);

    auto *client = ctx.registry->GetComponent<ClientComponent>(playerID);
    if (client) {
      json jsonData = {{"skill", skillName}, {"new_level", newLevel}};

      GameMessage msg;
      msg.type = "skill_up";
      msg.consoleText = "&YYour " + skillName + " mastery increases to level " +
                        std::to_string(newLevel) + "!&X";
      msg.jsonData = jsonData.dump();
      client->QueueGameMessage(msg);
    }

    std::string skillKey = ctx.factories->skills.GetSkillKey(skillID);
    SkillLevelUpEventData eventData = {playerID, skillKey, newLevel};
    EventContext eventContext;
    eventContext.data = eventData;
    ctx.eventBus->Publish(EventType::SkillLevelUp, eventContext);

    PowerCalculator::RecalculateDerivedStats(ctx.registry.get(), playerID);
  }
}
void GrantKillBonus(GameContext &ctx, int playerID, int skillID) {
  int effectiveSkill = skillID;
  if (effectiveSkill <= 0) {
    auto *equipment = ctx.registry->GetComponent<EquipmentComponent>(playerID);
    if (equipment) {
      auto slotIt = equipment->slots.find(EquipmentSlot::MainArm);
      if (slotIt != equipment->slots.end() && slotIt->second > 0) {
        auto *itemComp =
            ctx.registry->GetComponent<ItemComponent>(slotIt->second);
        if (itemComp && itemComp->primarySkillId != -1) {
          effectiveSkill = itemComp->primarySkillId;
        }
      }
    }
  }
  if (effectiveSkill <= 0)
    return;
  Grant(ctx, playerID, effectiveSkill, XP_KILL_BONUS);
}

} // namespace SkillXP

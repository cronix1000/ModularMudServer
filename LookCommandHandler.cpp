#include "LookCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "ActorComponents.h"
#include "CombatComponents.h"
#include "InteractableComponents.h"
#include "InventoryComponents.h"
#include "MovementComponents.h"
#include "PlayerComponents.h"
#include "PresentationComponents.h"
#include "StatsComponents.h"
#include "WorldComponents.h"
#include "EntityResolver.h"
#include "GMCPModules.h"
#include "InteractionMessage.h"
#include "TextHelperFunctions.h"
#include "TerrainDef.h"
#include "WorldManager.h"

#include <nlohmann/json.hpp>

#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

    const RoomIdentityComponent* FindRoom(Registry& r, int roomId) {
        for (EntityID e : r.view<RoomIdentityComponent>()) {
            auto* id = r.GetComponent<RoomIdentityComponent>(e);
            if (id && id->roomId == roomId) return id;
        }
        return nullptr;
    }

    std::string LowerCopy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    void QueueText(GameContext& ctx, ClientConnection* client, const std::string& text);

    // Handles "examine <terrain>": matches a single layout character or a
    // terrain name (case-insensitive) among the symbols used in this room.
    // Returns true when the query was a terrain tile and a reply was queued.
    bool TryExamineTerrain(GameContext& ctx, ClientConnection* client, PositionComponent* pos, const std::vector<std::string>& params) {
        if (!ctx.worldManager) return false;
        const RoomLayoutComponent* layout = ctx.worldManager->GetRoomLayout(pos->roomId);
        if (!layout || layout->width <= 0 || layout->height <= 0 || params.empty()) return false;

        const RoomIdentityComponent* room = FindRoom(*ctx.registry, pos->roomId);
        const std::string* regionId = room && !room->regionId.empty() ? &room->regionId : nullptr;

        std::string query = params[0];
        for (size_t i = 1; i < params.size(); ++i) query += " " + params[i];
        std::string lower = LowerCopy(query);

        const TerrainDef* match = nullptr;
        for (int y = 0; y < layout->height && !match; ++y) {
            for (int x = 0; x < layout->width && !match; ++x) {
                char c = static_cast<char>(layout->GetTerrain(x, y));
                const TerrainDef& def = GetTerrainFor(c, regionId);
                if (query.size() == 1 && c == query[0]) {
                    match = &def;
                } else if (!lower.empty() && LowerCopy(def.name) == lower) {
                    match = &def;
                }
            }
        }
        if (!match) return false;

        std::ostringstream msg;
        msg << match->color << match->symbol << "&w " << match->name << "\r\n";
        if (!match->description.empty()) msg << match->description << "\r\n";
        msg << "&x(terrain";
        if (match->blocks_move) msg << ", blocks movement";
        if (match->blocks_sight) msg << ", blocks sight";
        if (match->move_cost > 1) msg << ", move cost " << match->move_cost;
        msg << ")&w\r\n";
        QueueText(ctx, client, TextHelperFunctions::Colorize(msg.str()));
        return true;
    }

    void QueueText(GameContext& ctx, ClientConnection* client, const std::string& text) {
        if (!client) return;
        EntityID pid = client->playerEntityID;
        auto* cli = ctx.registry->GetComponent<ClientComponent>(pid);
        if (!cli) return;
        GameMessage msg("look", text);
        cli->QueueGameMessage(msg);
    }

    void QueueGmcp(GameContext& ctx, ClientConnection* client, const std::string& module, const std::string& text, const nlohmann::json& data) {
        if (!client) return;
        EntityID pid = client->playerEntityID;
        auto* cli = ctx.registry->GetComponent<ClientComponent>(pid);
        if (!cli) return;
        GameMessage msg;
        msg.type = module;
        msg.consoleText = text;
        msg.jsonData = data.dump();
        cli->QueueGameMessage(msg);
    }

    void QueueInspect(GameContext& ctx, ClientConnection* client, const std::string& module, const std::string& text, const nlohmann::json& data) {
        if (!client) return;
        EntityID pid = client->playerEntityID;
        auto* cli = ctx.registry->GetComponent<ClientComponent>(pid);
        if (!cli) return;
        if (cli->isWebClient) {
            QueueGmcp(ctx, client, module, text, data);
        } else if (!text.empty()) {
            cli->QueueGameMessage("look", text);
        }
    }

    bool IsAllDigits(const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
    }

    std::string StripBrackets(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size(); ) {
            if (i + 1 < text.size() && text[i] == '[' && text[i + 1] == '[') {
                size_t end = text.find("]]", i + 2);
                if (end != std::string::npos) {
                    out += text.substr(i + 2, end - (i + 2));
                    i = end + 2;
                    continue;
                }
            }
            out += text[i++];
        }
        return out;
    }

    std::string RenderRoomDescription(Registry& r, int roomId) {
        const RoomIdentityComponent* room = FindRoom(r, roomId);
        if (!room) return "";
        std::ostringstream out;
        out << room->name << "\r\n";
        out << StripBrackets(room->description) << "\r\n";
        return out.str();
    }

    const char* HealthSeverity(int hp, int maxHp) {
        if (maxHp <= 0) return "unscathed";
        float pct = static_cast<float>(hp) / static_cast<float>(maxHp);
        if (pct > 0.75f) return "healthy";
        if (pct > 0.5f)  return "slightly wounded";
        if (pct > 0.25f) return "wounded";
        return "near death";
    }

    std::string AppendStateLines(EntityID entity, Registry& r) {
        std::ostringstream out;
        if (auto* chest = r.GetComponent<ChestComponent>(entity)) {
            if (chest->is_locked) {
                out << "It is locked.\r\n";
            } else if (chest->is_open) {
                if (chest->uses_remaining > 0) {
                    out << "It is open.\r\n";
                } else {
                    out << "It has been emptied.\r\n";
                }
            } else {
                out << "It is closed.\r\n";
            }
        }
        if (r.HasComponent<MobComponent>(entity)) {
            auto* stat = r.GetComponent<StatComponent>(entity);
            if (stat && stat->MaxHealth > 0) {
                out << "It looks " << HealthSeverity(stat->Health, stat->MaxHealth) << ".\r\n";
            }
        }
        return out.str();
    }

    std::string RenderEntity(EntityID entity, Registry& r) {
        auto* name = r.GetComponent<NameComponent>(entity);
        auto* desc = r.GetComponent<DescriptionComponent>(entity);
        if (!name) return "";
        if (desc && desc->hidden) return "";

        std::unordered_map<std::string, std::string> tokens{
            {"name", name->displayName},
        };
        auto resolved = InteractionMessage::Resolve(r, entity, "look", tokens);
        if (resolved) {
            std::ostringstream rOut;
            rOut << name->displayName << "\r\n";
            rOut << resolved->message << "\r\n";
            rOut << AppendStateLines(entity, r);
            return rOut.str();
        }

        std::ostringstream out;
        out << name->displayName << "\r\n";
        if (desc && !desc->description.empty()) {
            out << StripBrackets(desc->description) << "\r\n";
        }
        out << AppendStateLines(entity, r);
        return out.str();
    }

    std::string RenderBrief(EntityID entity, Registry& r) {
        auto* name = r.GetComponent<NameComponent>(entity);
        auto* desc = r.GetComponent<DescriptionComponent>(entity);
        if (!name) return "";
        if (desc && desc->hidden) return "";

        std::ostringstream out;
        out << "  " << name->displayName << " — ";
        if (desc && !desc->description.empty()) {
            out << StripBrackets(desc->description);
        }
        out << "\r\n";
        return out.str();
    }

    struct InspectOutput {
        std::string text;
        nlohmann::json json;
    };

    bool KnownStatKey(const std::string& k) {
        return k == "strength" || k == "dexterity" || k == "intelligence" || k == "wisdom"
            || k == "health" || k == "hp" || k == "max_hp" || k == "max_health"
            || k == "armour" || k == "armor" || k == "magicDefense" || k == "magic_defense"
            || k == "attackDamage" || k == "magicDamage"
            || k == "attackSpeed" || k == "mana"
            || k == "perception";
    }

    InspectOutput inspectItem(Registry& r, EntityID entity) {
        InspectOutput out;
        auto* name = r.GetComponent<NameComponent>(entity);
        auto* desc = r.GetComponent<DescriptionComponent>(entity);
        auto* item  = r.GetComponent<ItemComponent>(entity);
        auto* weapon = r.GetComponent<WeaponComponent>(entity);
        auto* armour = r.GetComponent<ArmourComponent>(entity);
        auto* mods   = r.GetComponent<StatModifierComponent>(entity);

        if (!name) return out;
        if (desc && desc->hidden) return out;

        std::ostringstream txt;
        txt << "You look closer at " << name->displayName << ".\r\n";
        if (desc && !desc->description.empty()) {
            txt << StripBrackets(desc->description) << "\r\n";
        }

        std::string kind = "item";
        nlohmann::json stats = nlohmann::json::object();

        if (weapon) {
            kind = "weapon";
            txt << "  Damage: " << weapon->minDamage << "-" << weapon->maxDamage
                << " " << weapon->damageType << "\r\n";
            stats["minDamage"] = weapon->minDamage;
            stats["maxDamage"] = weapon->maxDamage;
            stats["damageType"] = weapon->damageType;

            int strPct = int(weapon->strScaling * 100.0f);
            int dexPct = int(weapon->dexScaling * 100.0f);
            int intPct = int(weapon->intScaling * 100.0f);
            if (strPct || dexPct || intPct) {
                txt << "  Scaling:";
                if (strPct) { txt << " str +" << strPct << "%"; }
                if (dexPct) { txt << " dex +" << dexPct << "%"; }
                if (intPct) { txt << " int +" << intPct << "%"; }
                txt << "\r\n";
                stats["strScaling"] = weapon->strScaling;
                stats["dexScaling"] = weapon->dexScaling;
                stats["intScaling"] = weapon->intScaling;
            }

            if (weapon->alignmentScaling > 0.0f && weapon->alignmentType != "none") {
                txt << "  Alignment: " << weapon->alignmentType
                    << " x" << weapon->alignmentScaling << "\r\n";
                stats["alignmentType"] = weapon->alignmentType;
                stats["alignmentScaling"] = weapon->alignmentScaling;
            }

            txt << "  Range: " << weapon->maxRange << "\r\n";
            txt << "  Base windup: " << weapon->baseWindup << "s\r\n";
            if (!weapon->scalingStat.empty()) {
                int pct = int(weapon->scalingFactor * 100.0f);
                txt << "  Speed scales with: " << weapon->scalingStat
                    << " (" << pct << "% per point)\r\n";
                stats["scalingStat"] = weapon->scalingStat;
                stats["scalingFactor"] = weapon->scalingFactor;
            }
            stats["baseWindup"] = weapon->baseWindup;
            stats["maxRange"] = weapon->maxRange;

            if (!weapon->defaultSkillTemplate.empty()) {
                txt << "  Default skill: " << weapon->defaultSkillTemplate << "\r\n";
                stats["defaultSkillTemplate"] = weapon->defaultSkillTemplate;
            }
        }
        else if (armour) {
            kind = "armour";
            txt << "  Defense: " << armour->defense << "\r\n";
            txt << "  Magic defense: " << armour->magicDefense << "\r\n";
            txt << "  Type: " << armour->type << "\r\n";
            if (item && !item->allowedSlots.empty()) {
                txt << "  Slots: ";
                for (size_t i = 0; i < item->allowedSlots.size(); ++i) {
                    if (i > 0) txt << ", ";
                    txt << TextHelperFunctions::SlotToString(item->allowedSlots[i]);
                }
                txt << "\r\n";
                stats["slot"] = static_cast<int>(item->allowedSlots.front());
                stats["allowedSlots"] = nlohmann::json::array();
                for (EquipmentSlot s : item->allowedSlots) {
                    stats["allowedSlots"].push_back(static_cast<int>(s));
                }
            }
            stats["defense"] = armour->defense;
            stats["magicDefense"] = armour->magicDefense;
            stats["type"] = armour->type;
        }

        if (item) {
            txt << "  Weight: " << item->weight << "\r\n";
            txt << "  Value: " << item->value << "g\r\n";
            if (!item->is_gettable)   txt << "  You cannot pick this up.\r\n";
            if (item->is_equippable)  txt << "  Equippable.\r\n";
            if (item->primarySkillId >= 0) {
                txt << "  Grants skill #" << item->primarySkillId << "\r\n";
            }
            if (!item->extraSkillIds.empty()) {
                txt << "  Extra skills:";
                for (int sid : item->extraSkillIds) {
                    txt << " #" << sid;
                }
                txt << "\r\n";
            }
            stats["weight"] = item->weight;
            stats["value"] = item->value;
            stats["isGettable"] = item->is_gettable;
            stats["isEquippable"] = item->is_equippable;
            stats["primarySkillId"] = item->primarySkillId;
            stats["extraSkillIds"] = item->extraSkillIds;
        }

        if (mods) {
            std::ostringstream mline;
            bool any = false;
            for (const auto& [k, v] : mods->modifiers) {
                if (!KnownStatKey(k)) continue;
                if (!any) {
                    mline << "  Modifiers:";
                    any = true;
                }
                mline << " " << k << " " << (v >= 0 ? "+" : "") << v;
                stats["mod_" + k] = v;
            }
            if (any) {
                mline << "\r\n";
                txt << mline.str();
            }
        }

        out.text = txt.str();
        out.json = {
            {"kind", kind},
            {"id", static_cast<int>(entity)},
            {"name", name->displayName},
            {"stats", stats}
        };
        if (desc) out.json["description"] = desc->description;
        return out;
    }

    InspectOutput inspectEnemy(Registry& r, EntityID entity) {
        InspectOutput out;
        auto* name = r.GetComponent<NameComponent>(entity);
        auto* desc = r.GetComponent<DescriptionComponent>(entity);
        auto* stat = r.GetComponent<StatComponent>(entity);
        if (!name) return out;
        if (desc && desc->hidden) return out;

        std::ostringstream txt;
        txt << "You take a wary step and examine " << name->displayName << ".\r\n";
        if (desc && !desc->description.empty()) {
            txt << StripBrackets(desc->description) << "\r\n";
        }

        if (stat && stat->MaxHealth > 0) {
            txt << "  HP: " << stat->Health << "/" << stat->MaxHealth
                << " (" << HealthSeverity(stat->Health, stat->MaxHealth) << ")\r\n";
        }

        out.text = txt.str();
        out.json = {
            {"kind", "mob"},
            {"id", static_cast<int>(entity)},
            {"name", name->displayName},
            {"hp", stat ? stat->Health : 0},
            {"max_hp", stat ? stat->MaxHealth : 0}
        };
        if (desc) out.json["description"] = desc->description;
        return out;
    }

    InspectOutput RenderInspect(Registry& r, EntityID entity) {
        if (r.HasComponent<MobComponent>(entity)) {
            return inspectEnemy(r, entity);
        }
        if (r.GetComponent<WeaponComponent>(entity)
         || r.GetComponent<ArmourComponent>(entity)
         || r.GetComponent<ItemComponent>(entity)) {
            return inspectItem(r, entity);
        }
        InspectOutput fallback;
        fallback.text = RenderEntity(entity, r);
        fallback.json = nlohmann::json::object();
        return fallback;
    }
}


void LookCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.RegisterWithAliases("look", HandleLook, {"l"}, PermissionLevel::Player);
    registry.RegisterWithAliases("examine", HandleExamine, { "ex" }, PermissionLevel::Player);
    registry.RegisterWithAliases("inspect", HandleExamine, { "i" }, PermissionLevel::Player);
}

CommandResult LookCommandHandler::HandleLook(ClientConnection* client,
                                             const std::vector<std::string>& params,
                                             GameContext& ctx) {
    EntityID playerID = client->playerEntityID;
    auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
    if (!playerPos) {
        return CommandResult::Failure("You are nowhere.");
    }

    std::string roomText = RenderRoomDescription(*ctx.registry, playerPos->roomId);
    if (!roomText.empty()) QueueText(ctx, client, roomText);

    if (params.empty()) {
        return CommandResult::Success();
    }

    if (params.size() == 1 && IsAllDigits(params[0])) {
        auto* intent = ctx.registry->GetComponent<TargetingIntentComponent>(playerID);
        if (intent && intent->elapsedTime < intent->promptExpireTime &&
            !intent->candidates.empty()) {
            int idx = std::stoi(params[0]);
            if (idx >= 1 && idx <= static_cast<int>(intent->candidates.size())) {
                EntityID chosen = intent->candidates[idx - 1];
                ctx.registry->RemoveComponent<TargetingIntentComponent>(playerID);
                std::string rendered = RenderEntity(chosen, *ctx.registry);
                if (!rendered.empty()) QueueText(ctx, client, rendered);
                return CommandResult::Success();
            }
        }
    }

    if (params.size() == 1 && params[0] == "around") {
        auto nearby = ctx.entityFind->ScanInShape(
            playerPos->roomId, playerPos->x, playerPos->y,
            EntityResolver::ShapeAround());
        std::ostringstream msg;
        msg << "You see here:\r\n";
        for (EntityID e : nearby) {
            if (e == playerID) continue;
            std::string line = RenderBrief(e, *ctx.registry);
            if (!line.empty()) msg << line;
        }
        if (msg.str() == "You see here:\r\n") {
            msg << "  (nothing of note nearby)\r\n";
        }
        QueueText(ctx, client, msg.str());
        return CommandResult::Success();
    }

    auto hits = ctx.entityFind->FindInRoom(playerPos->roomId, params);
    if (hits.empty()) {
        if (TryExamineTerrain(ctx, client, playerPos, params)) {
            return CommandResult::Success();
        }
        return CommandResult::Failure("You don't see that here.");
    }
    if (hits.size() == 1) {
        EntityID target = hits[0];
        auto* desc = ctx.registry->GetComponent<DescriptionComponent>(target);
        if (desc && desc->hidden) {
            return CommandResult::Failure("You don't see that here.");
        }
        std::string rendered = RenderEntity(target, *ctx.registry);
        if (!rendered.empty()) QueueText(ctx, client, rendered);
        return CommandResult::Success();
    }

    TargetingIntentComponent intent;
    intent.sourceID = playerID;
    intent.targetName = "";
    intent.candidates = hits;
    intent.queryTokens = params;
    intent.roomId = playerPos->roomId;
    intent.promptExpireTime = 30.0f;
    intent.elapsedTime = 0.0f;
    ctx.registry->AddComponent<TargetingIntentComponent>(playerID, intent);

    std::ostringstream msg;
    msg << "Which one did you mean?\r\n";
    for (size_t i = 0; i < hits.size(); ++i) {
        auto* name = ctx.registry->GetComponent<NameComponent>(hits[i]);
        msg << "  " << (i + 1) << ". " << (name ? name->displayName : "something") << "\r\n";
    }
    msg << "Pick 1-" << hits.size() << " or refine your search.\r\n";
    QueueText(ctx, client, msg.str());
    return CommandResult::Success();
}



CommandResult LookCommandHandler::HandleExamine(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
    if (params.empty()) {
        return CommandResult::Failure("Examine what?");
    }

    EntityID playerID = client->playerEntityID;
    auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
    if (!playerPos) {
        return CommandResult::Failure("You are nowhere.");
    }

    if (params.size() == 1 && IsAllDigits(params[0])) {
        auto* intent = ctx.registry->GetComponent<TargetingIntentComponent>(playerID);
        if (intent && intent->elapsedTime < intent->promptExpireTime &&
            !intent->candidates.empty()) {
            int idx = std::stoi(params[0]);
            if (idx >= 1 && idx <= static_cast<int>(intent->candidates.size())) {
                EntityID chosen = intent->candidates[idx - 1];
                ctx.registry->RemoveComponent<TargetingIntentComponent>(playerID);
                auto inspected = RenderInspect(*ctx.registry, chosen);
                QueueInspect(ctx, client, gmcp::Entity_Inspect, inspected.text, inspected.json);
                return CommandResult::Success();
            }
        }
    }

    auto hits = ctx.entityFind->FindInRoom(playerPos->roomId, params);
    if (hits.empty()) {
        if (TryExamineTerrain(ctx, client, playerPos, params)) {
            return CommandResult::Success();
        }
        return CommandResult::Failure("You don't see that here.");
    }

    if (hits.size() == 1) {
        EntityID target = hits[0];
        auto* desc = ctx.registry->GetComponent<DescriptionComponent>(target);
        if (desc && desc->hidden) {
            return CommandResult::Failure("You don't see that here.");
        }
        auto inspected = RenderInspect(*ctx.registry, target);
        QueueInspect(ctx, client, gmcp::Entity_Inspect, inspected.text, inspected.json);
        return CommandResult::Success();
    }

    TargetingIntentComponent intent;
    intent.sourceID = playerID;
    intent.targetName = "";
    intent.candidates = hits;
    intent.queryTokens = params;
    intent.roomId = playerPos->roomId;
    intent.promptExpireTime = 30.0f;
    intent.elapsedTime = 0.0f;
    ctx.registry->AddComponent<TargetingIntentComponent>(playerID, intent);

    std::ostringstream msg;
    msg << "Which one did you mean?\r\n";
    for (size_t i = 0; i < hits.size(); ++i) {
        auto* name = ctx.registry->GetComponent<NameComponent>(hits[i]);
        msg << "  " << (i + 1) << ". " << (name ? name->displayName : "something") << "\r\n";
    }
    msg << "Pick 1-" << hits.size() << " or refine your search.\r\n";
    QueueText(ctx, client, msg.str());
    return CommandResult::Success();
}
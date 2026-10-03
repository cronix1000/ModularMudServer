#include "LookCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "NameComponent.h"
#include "DescriptionComponent.h"
#include "PositionComponent.h"
#include "ChestComponent.h"
#include "MobComponent.h"
#include "StatComponent.h"
#include "RoomComponents.h"
#include "TargetingIntentComponent.h"
#include "ClientComponent.h"
#include "EntityResolver.h"

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

    void QueueText(GameContext& ctx, ClientConnection* client, const std::string& text) {
        if (!client) return;
        EntityID pid = client->playerEntityID;
        auto* cli = ctx.registry->GetComponent<ClientComponent>(pid);
        if (!cli) return;
        GameMessage msg("look", text);
        cli->QueueGameMessage(msg);
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
                float pct = static_cast<float>(stat->Health) / static_cast<float>(stat->MaxHealth);
                out << "It looks ";
                if (pct > 0.75f) out << "healthy";
                else if (pct > 0.5f) out << "slightly wounded";
                else if (pct > 0.25f) out << "wounded";
                else out << "near death";
                out << ".\r\n";
            }
        }
        return out.str();
    }

    std::string RenderEntity(EntityID entity, Registry& r) {
        auto* name = r.GetComponent<NameComponent>(entity);
        auto* desc = r.GetComponent<DescriptionComponent>(entity);
        if (!name) return "";
        if (desc && desc->hidden) return "";

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
}

void LookCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.RegisterWithAliases("look", HandleLook, {"l"}, PermissionLevel::Player);
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
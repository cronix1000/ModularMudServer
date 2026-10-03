#include "ItemCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "InventoryComponent.h"
#include "PickupItemIntentComponent.h"
#include "EquipItemIntentComponent.h"
#include "ItemComponent.h"
#include "TargetingIntentComponent.h"
#include "ArmourComponent.h"
#include "TextHelperFunctions.h"
#include "EntityResolver.h"
#include <sstream>
#include <iomanip>
#include <cctype>


void ItemCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.RegisterWithAliases("pickup", HandlePickup, {"get", "take"}, PermissionLevel::Player);
    registry.RegisterWithAliases("drop", HandleDrop, {}, PermissionLevel::Player);
    registry.RegisterWithAliases("equip", HandleEquip, {"wear", "wield"}, PermissionLevel::Player);
    registry.RegisterWithAliases("inventory", HandleInventory, {"i", "inv"}, PermissionLevel::Player);
}

CommandResult ItemCommandHandler::HandlePickup(ClientConnection* client,
                                               const std::vector<std::string>& params,
                                               GameContext& ctx) {
    if (params.empty()) {
        return CommandResult::Failure("Pickup what?");
    }

    EntityID playerID = client->playerEntityID;
    auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
    if (!playerPos) {
        return CommandResult::Failure("You are nowhere.");
    }

    auto hits = ctx.entityFind->FindByShape(
        playerPos->roomId, playerPos->x, playerPos->y,
        params, EntityResolver::ShapeAround());

    if (hits.empty()) {
        return CommandResult::Failure("You don't see that here.");
    }

    if (hits.size() > 1) {
        TargetingIntentComponent intent;
        intent.sourceID = playerID;
        intent.targetName = "";
        intent.candidates = hits;
        intent.queryTokens = params;
        intent.roomId = playerPos->roomId;
        intent.promptExpireTime = 30.0f;
        intent.elapsedTime = 0.0f;
        ctx.registry->AddComponent<TargetingIntentComponent>(playerID, intent);

        std::ostringstream out;
        out << "Which one did you mean?\r\n";
        for (size_t i = 0; i < hits.size(); ++i) {
            auto* name = ctx.registry->GetComponent<NameComponent>(hits[i]);
            out << "  " << (i + 1) << ". " << (name ? name->displayName : "something") << "\r\n";
        }
        out << "Pick 1-" << hits.size() << " or refine your search.\r\n";
        return CommandResult::Success(out.str());
    }

    EntityID target = hits[0];
    if (!ctx.registry->HasComponent<ItemComponent>(target)) {
        return CommandResult::Failure("You can't pick that up.");
    }

    ctx.registry->AddComponent<PickupItemIntentComponent>(playerID, PickupItemIntentComponent{ target });
    return CommandResult::Success();
}

CommandResult ItemCommandHandler::HandleDrop(ClientConnection* client,
                                             const std::vector<std::string>& params,
                                             GameContext& ctx) {
    (void)client; (void)params; (void)ctx;
    return CommandResult::Failure("Drop command not yet implemented.");
}

CommandResult ItemCommandHandler::HandleEquip(ClientConnection* client,
                                              const std::vector<std::string>& params,
                                              GameContext& ctx) {
    if (params.empty()) {
        return CommandResult::Failure("Equip what?");
    }

    try {
        int actualID = std::stoi(params[0]);

        ctx.registry->AddComponent<EquipItemIntentComponent>(
            client->playerEntityID,
            EquipItemIntentComponent{ actualID }
        );

        return CommandResult::Success("You prepare to equip the item.");
    }
    catch (const std::exception&) {
        return CommandResult::Failure("Invalid item ID. Please provide a valid number.");
    }
}

CommandResult ItemCommandHandler::HandleInventory(ClientConnection* client,
                                                  const std::vector<std::string>& params,
                                                  GameContext& ctx) {
    (void)params;
    InventoryComponent* inv = ctx.registry->GetComponent<InventoryComponent>(client->playerEntityID);
    if (!inv) {
        return CommandResult::Failure("You have no inventory.");
    }

    std::ostringstream out;
    out << "===== [ TOWER INVENTORY ] =====\r\n";
    int n = 0;
    for (auto& itemId : inv->items) {
        n++;
        auto* name = ctx.registry->GetComponent<NameComponent>(itemId);
        auto* armour = ctx.registry->GetComponent<ArmourComponent>(itemId);
        out << std::setw(3) << n << " | "
            << std::setw(24) << (name ? name->displayName : "?") << " | "
            << std::setw(9);
        if (armour) {
            out << TextHelperFunctions::SlotToString(armour->slot);
        } else {
            out << "General";
        }
        out << " | item" << "\r\n";
    }
    return CommandResult::Success(out.str());
}
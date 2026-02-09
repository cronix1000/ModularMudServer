#pragma once
#include "GameState.h"
#include "GameEngine.h"
#include "ClientConnection.h"
#include "MoveIntentComponent.h"
#include "CommandInterpreter.h"
#include "DirtyFlagComponents.h"
#include "Registry.h"
#include "MenuType.h"
#include "GameContext.h"
#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "TextHelperFunctions.h"
#include <filesystem>
#include "WeaponComponent.h"
#include "ArmourComponent.h"
struct MenuOption {
    int id;
    std::string command; 
};

class MenuState : public GameState
{
public:
    GameContext& ctx; // Reference to the "World"
    std::vector<MenuOption> options;
    std::string headerAscii;
    std::unique_ptr<Command> currentCmd;
    MenuType type;
    int targetEntity;

public:
    MenuState(GameContext& context, MenuType type, int target)
        : ctx(context), type(type), targetEntity(target), currentCmd(std::make_unique<Command>())
    {
        switch (type) {
            case MenuType::Inventory:
            {
                BuildInventoryMenu(target);
            }
        }
    }

    ~MenuState() {

    }

    void OnEnter(ClientConnection* client) override {
        std::stringstream menuText;

        if (type == MenuType::Inventory) {
            menuText << "\n{Y}========== [ TOWER INVENTORY ] =========={x}\n";
            menuText << "  # | Item Name                | Slot      | Type\n";
            menuText << "----+--------------------------+-----------+-------\n";

            auto* inventory = ctx.registry->GetComponent<InventoryComponent>(client->playerEntityID);
            int itemNumber = 1;

            for (auto const& itemID : inventory->items) {
                auto* name = ctx.registry->GetComponent<NameComponent>(itemID);
                auto* weapon = ctx.registry->GetComponent<WeaponComponent>(itemID);
                auto* armour = ctx.registry->GetComponent<ArmourComponent>(itemID);

                // Determine display slot and color
                std::string slotName = "---";
                std::string itemType = "Misc";
                std::string color = "{w}"; // Default white

                if (weapon) {
                    slotName = "Off or Main";
                    itemType = "Weapon";
                    color = "{R}"; // Red for weapons
                }
                else if (armour) {
                    slotName = TextHelperFunctions::SlotToString(armour->slot);
                    itemType = "Armour";
                    color = "{B}"; // Blue for armour
                }

                // Formatting: #[number] | [Name] | [Slot] | [Type]
                menuText << " " << (itemNumber < 10 ? " " : "") << itemNumber << " | "
                    << color << std::left << std::setw(24) << name->displayName << "{x} | "
                    << std::setw(9) << slotName << " | "
                    << itemType << "\n";

                itemNumber++;
            }
            menuText << "---------------------------------------------------\n";
            menuText << "Usage: {G}Equip <#>{x} | {R}Drop <#>{x} | {C}Examine <#>{x}\n";
        }
        client->QueueMessage(menuText.str());
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
        std::string input = p[0];

        if (input == "exit" || input == "back") {
            client->PopState();
            return;
        }

        try {
            int choice = std::stoi(input) -1;

            if (choice >= 0 && choice < (int)options.size()) {
                MenuOption& option = options[choice];

                // 2. PREPARE THE COMMAND
                currentCmd->CommandString = option.command;

                currentCmd->Parameters.clear();

                currentCmd->Parameters.push_back(std::to_string(option.id));

                // 3. EXECUTE
                ctx.interpreter.get()->Interpret(client, currentCmd.get());
            }
            else {
                client->QueueMessage("Invalid choice.\n");
            }
        }
        catch (...) {
            client->QueueMessage("Please enter a number or 'exit'.\n");
        }
    }

    void BuildInventoryMenu(int playerID) {
        options.clear();
        auto* inv = ctx.registry->GetComponent<InventoryComponent>(playerID);

        int displayIdx = 1;
        for (int itemId : inv->items) {
            auto* item = ctx.registry->GetComponent<ItemComponent>(itemId);

            options.push_back({ itemId, "equip" });
            displayIdx++;
        }
    }
};
#include "ShopFactory.h"
#include "Registry.h"
#include "PlayerVariablesComponent.h"
#include "ItemFactory.h"
#include "FactoryManager.h"
#include "ItemComponent.h"
#include "InventoryComponent.h"
#include "DirtyFlagComponents.h"
#include "Tags.h"
#include <iostream>
#include <chrono>
#include <algorithm>

void ShopFactory::LoadShopsFromJson(const json& data) {
	std::cout << "[ShopFactory] Loading shop keepers..." << std::endl;
	int n = 0;
	if (!data.is_object()) return;

	if (data.contains("shops") && data["shops"].is_object()) {
		for (auto& [key, j] : data["shops"].items()) {
			ShopKeeper k;
			k.id = key;
			k.mobId = j.value("mob_id", "");
			k.markup = j.value("markup", 1.0);
			k.markdown = j.value("markdown", 1.0);
			k.openHour = j.value("open_hour", 0);
			k.closeHour = j.value("close_hour", 24);
			k.shopType = j.value("shop_type", "general");

			if (j.contains("inventory") && j["inventory"].is_object()) {
				for (auto& [tplId, itemJson] : j["inventory"].items()) {
					ShopItem item;
					item.templateId = tplId;
					item.maxStock = itemJson.value("max_stock", -1);
					item.currentStock = itemJson.value("current_stock", -1);
					item.restockSeconds = itemJson.value("restock_seconds", 600);
					item.priceOverride = itemJson.value("price_override", -1);
					k.inventory[tplId] = item;
				}
			}
			shops[key] = std::move(k);
			++n;
		}
	}
	std::cout << "[ShopFactory] Loaded " << n << " shop keepers." << std::endl;
}

int ShopFactory::GetPlayerGold(int playerID, const std::string& slot) const {
	std::string key = slot.empty() ? "gold" : slot;
	auto* vars = ctx.registry->GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) return 0;
	auto it = vars->intVars.find(key);
	return (it != vars->intVars.end()) ? it->second : 0;
}

bool ShopFactory::SetGold(int playerID, int amount, const std::string& slot) {
	if (amount < 0) return false;
	std::string key = slot.empty() ? "gold" : slot;
	auto* vars = ctx.registry->GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) {
		ctx.registry->AddComponent<PlayerVariablesComponent>(playerID);
		vars = ctx.registry->GetComponent<PlayerVariablesComponent>(playerID);
		if (!vars) return false;
	}
	vars->intVars[key] = amount;
	return true;
}

int ShopFactory::QuoteBuyPrice(const std::string& keeperId, const std::string& templateId) const {
	auto kit = shops.find(keeperId);
	if (kit == shops.end()) return -1;
	auto iit = kit->second.inventory.find(templateId);
	if (iit == kit->second.inventory.end()) return -1;
	if (iit->second.priceOverride > 0) return iit->second.priceOverride;

	int baseValue = 0;
	if (ctx.factories && ctx.factories->items.itemTemplates.count(templateId)) {
		baseValue = ctx.factories->items.itemTemplates.at(templateId).value;
	}
	int price = static_cast<int>(baseValue * kit->second.markup);
	if (price < 1) price = 1;
	return price;
}

int ShopFactory::QuoteSellPrice(const std::string& keeperId, const std::string& templateId) const {
	auto kit = shops.find(keeperId);
	if (kit == shops.end()) return -1;
	auto iit = kit->second.inventory.find(templateId);
	if (iit == kit->second.inventory.end()) {
		if (ctx.factories && ctx.factories->items.itemTemplates.count(templateId)) {
			return std::max<int>(1, ctx.factories->items.itemTemplates.at(templateId).value / 4);
		}
		return 1;
	}
	if (iit->second.priceOverride > 0) {
		return std::max<int>(1, iit->second.priceOverride / 4);
	}
	int baseValue = 0;
	if (ctx.factories && ctx.factories->items.itemTemplates.count(templateId)) {
		baseValue = ctx.factories->items.itemTemplates.at(templateId).value;
	}
	return std::max<int>(1, static_cast<int>(baseValue * kit->second.markdown));
}

bool ShopFactory::Buy(int playerID, const std::string& keeperId, const std::string& templateId, int qty) {
	if (qty <= 0) return false;
	auto kit = shops.find(keeperId);
	if (kit == shops.end()) return false;
	auto iit = kit->second.inventory.find(templateId);
	if (iit == kit->second.inventory.end()) return false;
	if (iit->second.currentStock >= 0 && iit->second.currentStock < qty) return false;

	int unitPrice = QuoteBuyPrice(keeperId, templateId);
	int totalCost = unitPrice * qty;
	int playerGold = GetPlayerGold(playerID);
	if (playerGold < totalCost) return false;

	if (!ctx.factories) return false;
	auto* inventory = ctx.registry->GetComponent<InventoryComponent>(playerID);
	if (!inventory) return false;

	for (int i = 0; i < qty; ++i) {
		if (inventory->items.size() >= static_cast<size_t>(inventory->max_slots)) return false;
		int newItem = ctx.factories->items.CreateItem(templateId, json::object());
		if (newItem <= 0) return false;
		inventory->items.push_back(newItem);
	}

	SetGold(playerID, playerGold - totalCost);
	if (iit->second.currentStock >= 0) {
		iit->second.currentStock -= qty;
	}
	ctx.registry->AddComponent<InventoryChangedComponent>(playerID);
	return true;
}

bool ShopFactory::Sell(int playerID, const std::string& keeperId, const std::string& templateId, int qty) {
	if (qty <= 0) return false;
	auto kit = shops.find(keeperId);
	if (kit == shops.end()) return false;

	auto* inventory = ctx.registry->GetComponent<InventoryComponent>(playerID);
	if (!inventory) return false;

	int found = 0;
	std::vector<int> indices;
	for (size_t i = 0; i < inventory->items.size() && found < qty; ++i) {
		auto* itemComp = ctx.registry->GetComponent<ItemComponent>(inventory->items[i]);
		if (itemComp && itemComp->templateName == templateId) {
			indices.push_back(static_cast<int>(i));
			++found;
		}
	}
	if (found < qty) return false;

	std::sort(indices.rbegin(), indices.rend());
	for (int idx : indices) {
		int itemId = inventory->items[idx];
		inventory->items.erase(inventory->items.begin() + idx);
		ctx.registry->AddComponent<DestroyTag>(itemId, DestroyTag{});
	}

	int unitPrice = QuoteSellPrice(keeperId, templateId);
	SetGold(playerID, GetPlayerGold(playerID) + unitPrice * qty);

	auto iit = kit->second.inventory.find(templateId);
	if (iit != kit->second.inventory.end() && iit->second.maxStock > 0
	    && iit->second.currentStock >= 0 && iit->second.currentStock < iit->second.maxStock) {
		iit->second.currentStock = std::min<int>(iit->second.maxStock, iit->second.currentStock + qty);
	}

	ctx.registry->AddComponent<InventoryChangedComponent>(playerID);
	return true;
}
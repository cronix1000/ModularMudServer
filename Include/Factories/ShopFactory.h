#pragma once
#include "GameContext.h"
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ShopItem {
	std::string templateId;
	int maxStock = -1;
	int currentStock = -1;
	int restockSeconds = 600;
	int priceOverride = -1;
};

struct ShopKeeper {
	std::string id;
	std::string mobId;
	double markup = 1.0;
	double markdown = 1.0;
	int openHour = 0;
	int closeHour = 24;
	std::string shopType;
	std::map<std::string, ShopItem> inventory;
};

class ShopFactory {
public:
	GameContext& ctx;
	std::map<std::string, ShopKeeper> shops;

	ShopFactory(GameContext& g) : ctx(g) {}

	void LoadShopsFromJson(const json& data);

	bool Buy(int playerID, const std::string& keeperId, const std::string& templateId, int qty);
	bool Sell(int playerID, const std::string& keeperId, const std::string& templateId, int qty);
	int QuoteBuyPrice(const std::string& keeperId, const std::string& templateId) const;
	int QuoteSellPrice(const std::string& keeperId, const std::string& templateId) const;
	int GetPlayerGold(int playerID, const std::string& slot = "") const;
	bool SetGold(int playerID, int amount, const std::string& slot = "");
};
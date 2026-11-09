#pragma once
#include "GameContext.h"
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct FactionDef {
	std::string id;
	std::string name;
	std::string description;
	std::string ideology;
	std::map<std::string, int> relations;
};

struct FactionStanding {
	std::string factionId;
	int standing = 0;
	long long updatedAt = 0;
};

class FactionFactory {
public:
	GameContext& ctx;
	std::map<std::string, FactionDef> factions;

	FactionFactory(GameContext& g) : ctx(g) {}

	void LoadFactionsFromJson(const json& data);

	int GetStanding(int playerID, const std::string& factionId) const;
	bool SetStanding(int playerID, const std::string& factionId, int value);
	int AdjustStanding(int playerID, const std::string& factionId, int delta);
	const FactionDef* GetFaction(const std::string& factionId) const;
};
#include "FactionFactory.h"
#include "GameContext.h"
#include "IDatabase.h"
#include "EventBus.h"
#include <iostream>
#include <algorithm>

void FactionFactory::LoadFactionsFromJson(const json& data) {
	std::cout << "[FactionFactory] Loading factions..." << std::endl;
	int n = 0;
	if (!data.is_object()) return;

	if (data.contains("factions") && data["factions"].is_object()) {
		for (auto& [key, j] : data["factions"].items()) {
			FactionDef f;
			f.id = key;
			f.name = j.value("name", key);
			f.description = j.value("description", "");
			f.ideology = j.value("ideology", "");
			if (j.contains("relations") && j["relations"].is_object()) {
				for (auto& [other, val] : j["relations"].items()) {
					if (val.is_number_integer()) {
						f.relations[other] = val.get<int>();
					}
				}
			}
			factions[key] = std::move(f);
			++n;
		}
	}

	std::cout << "[FactionFactory] Loaded " << n << " factions." << std::endl;
}

int FactionFactory::GetStanding(int playerID, const std::string& factionId) const {
	if (!ctx.db) return 0;
	return ctx.db->GetFactionStanding(playerID, factionId);
}

bool FactionFactory::SetStanding(int playerID, const std::string& factionId, int value) {
	if (!ctx.db) return false;
	if (factions.find(factionId) == factions.end()) {
		std::cerr << "[FactionFactory] SetStanding: unknown faction '" << factionId << "'" << std::endl;
		return false;
	}
	int oldStanding = ctx.db->GetFactionStanding(playerID, factionId);
	if (!ctx.db->SetFactionStanding(playerID, factionId, value)) {
		return false;
	}
	if (ctx.eventBus) {
		EventContext ctxData;
		ctxData.data = FactionChangeEventData{ playerID, factionId, oldStanding, value };
		ctx.eventBus->Publish(EventType::FactionChange, ctxData);
	}
	return true;
}

int FactionFactory::AdjustStanding(int playerID, const std::string& factionId, int delta) {
	int current = GetStanding(playerID, factionId);
	int newVal = std::clamp(current + delta, -1000, 1000);
	if (SetStanding(playerID, factionId, newVal)) {
		return newVal;
	}
	return current;
}

const FactionDef* FactionFactory::GetFaction(const std::string& factionId) const {
	auto it = factions.find(factionId);
	return (it != factions.end()) ? &it->second : nullptr;
}
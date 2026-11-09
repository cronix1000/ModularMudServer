#include "StatsCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "Component.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {
	std::string PadRight(const std::string& s, size_t width) {
		if (s.size() >= width) return s;
		return s + std::string(width - s.size(), ' ');
	}
}

void StatsCommandHandler::RegisterAll(CommandRegistry& registry) {
	registry.RegisterWithAliases("stats", HandleStats, {"score", "st"}, PermissionLevel::Player);
}

CommandResult StatsCommandHandler::HandleStats(ClientConnection* client,
                                               const std::vector<std::string>& params,
                                               GameContext& ctx) {
	EntityID playerID = client->playerEntityID;
	auto* clientComp = ctx.registry->GetComponent<ClientComponent>(playerID);
	if (!clientComp) return CommandResult::Failure("You have no body.");

	auto* stats = ctx.registry->GetComponent<StatComponent>(playerID);
	auto* nameComp = ctx.registry->GetComponent<NameComponent>(playerID);
	auto* holder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);

	std::string name = nameComp ? nameComp->displayName : "You";
	std::string sheet = "&W" + name + "&X\r\n";

	json jsonData = json::object();

	if (stats) {
		sheet += "&GHP &w" + std::to_string(stats->Health) + "/" + std::to_string(stats->MaxHealth) +
		         "   &CMana &w" + std::to_string(stats->Mana) + "&X\r\n";
		sheet += "&YPower &w" + std::to_string(stats->Power) +
		         "   &MMagic Power &w" + std::to_string(stats->MagicPower) + "&X\r\n";
		sheet += "&wStr " + std::to_string(stats->Strength) +
		         "  Dex " + std::to_string(stats->Dexterity) +
		         "  Int " + std::to_string(stats->Intelligence) +
		         "  Wis " + std::to_string(stats->Wisdom) + "\r\n";
		sheet += "&wArmour " + std::to_string(stats->Armour) +
		         "  Magic Defence " + std::to_string(stats->MagicDefense) + "\r\n";

		jsonData["hp"] = stats->Health;
		jsonData["max_hp"] = stats->MaxHealth;
		jsonData["mana"] = stats->Mana;
		jsonData["power"] = stats->Power;
		jsonData["magic_power"] = stats->MagicPower;
	}

	if (holder && !holder->skills.empty()) {
		std::vector<const std::pair<const int, SkillProgress>*> entries;
		for (auto const& entry : holder->skills) {
			entries.push_back(&entry);
		}
		std::sort(entries.begin(), entries.end(),
		          [](const auto* a, const auto* b) {
			          if (a->second.level != b->second.level) return a->second.level > b->second.level;
			          return a->second.uses > b->second.uses;
		          });

		sheet += "&WSkills:&X\r\n";
		json skillsJson = json::object();
		for (const auto* entry : entries) {
			auto* skillDef = ctx.registry->GetComponent<SkillDefinitionComponent>(entry->first);
			std::string skillName = skillDef ? skillDef->name : "Unknown";
			const SkillProgress& progress = entry->second;
			int needed = XPForNextLevel(progress.level);
			int filled = 0;
			if (needed > 0) filled = (std::min)(10, progress.xp * 10 / needed);
			std::string bar(filled, '#');
			bar += std::string(10 - filled, '.');

			sheet += "  &w" + PadRight(skillName, 18) +
			         " lvl " + std::to_string(progress.level) +
			         "  [" + bar + "] " + std::to_string(progress.xp) + "/" +
			         std::to_string(needed) + " xp\r\n";

			skillsJson[skillName] = {{"level", progress.level},
			                         {"xp", progress.xp},
			                         {"uses", progress.uses}};
		}
		jsonData["skills"] = skillsJson;
	} else {
		sheet += "&WSkills: &wnone yet - go use one!&X\r\n";
	}

	GameMessage msg;
	msg.type = "stats";
	msg.consoleText = sheet;
	msg.jsonData = jsonData.dump();
	clientComp->QueueGameMessage(msg);
	return CommandResult::Success();
}

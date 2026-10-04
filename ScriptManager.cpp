#include "ScriptManager.h"
#include "Registry.h"
#include "Component.h"
#include "ClientConnection.h"
#include "InteractableContext.h"
#include "SkillContext.h"
#include "EventBus.h"
#include "PlayerVariablesComponent.h"
#include "ClientComponent.h"
#include "GameContext.h"
#include "MetaComponent.h"
#include "FactionFactory.h"
#include "ShopFactory.h"
#include "FactoryManager.h"
#include <iostream>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include "sol/sol.hpp"

namespace fs = std::filesystem;

sol::table JsonToLua(sol::state& lua, const nlohmann::json& j) {
	sol::table tbl = lua.create_table();
	if (!j.is_object()) {
		return tbl;
	}
	for (auto it = j.begin(); it != j.end(); ++it) {
		const std::string& key = it.key();
		const nlohmann::json& val = it.value();
		if (val.is_string()) {
			tbl[key] = val.get<std::string>();
		} else if (val.is_boolean()) {
			tbl[key] = val.get<bool>();
		} else if (val.is_number_integer()) {
			tbl[key] = val.get<int>();
		} else if (val.is_number_float()) {
			tbl[key] = val.get<double>();
		} else if (val.is_null()) {
			tbl[key] = sol::nil;
		} else if (val.is_object()) {
			tbl[key] = JsonToLua(lua, val);
		} else {
			tbl[key] = sol::nil;
		}
	}
	return tbl;
}


ScriptManager::ScriptManager(Registry& r) : registry(r) {
	// Initialize lua state in constructor
	lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table);

	lua.new_usertype<StatComponent>("Stats",
		"hp", &StatComponent::Health,
		"strength", &StatComponent::Strength,
		"armour", &StatComponent::Armour
	);

	lua.new_usertype<SkillContext>("SkillContext",
		"source", &SkillContext::sourceID,
		"target", &SkillContext::targetID,
		"skillID", &SkillContext::skillID,
		"mastery", &SkillContext::masteryLevel,
		"power", &SkillContext::basePower
	);

	lua.new_usertype<InteractableContext>("InteractableContext",
		"interactableID", &InteractableContext::interactableID,
		"userID", &InteractableContext::userID,
		"roomID", &InteractableContext::roomID,
		"x", &InteractableContext::x,
		"y", &InteractableContext::y,
		"action", &InteractableContext::action,
		"parameters", &InteractableContext::parameters
	);

	//init();
}

void ScriptManager::init() {
	load_script("scripts/interactables/interactables_master.lua");
	load_script("scripts/skills/skills_master.lua");

	lua.set_function("send_to_char", [this](int player_id, const std::string& message) {
		auto* player = GetPlayer(player_id);
		if (player) {
			player->QueueMessage(message);
		}
		});

	lua.set_function("subscribe", [this](const std::string& event_name, sol::function callback) {
		event_listeners[event_name].push_back(callback);
		});

	lua.set_function("mark_dirty", [this](int entity_id) {
		registry.AddComponent<VitalsChangedComponent>(entity_id);
		});

	lua.set_function("get_stats", [this](int entity_id) -> StatComponent* {
		return registry.GetComponent<StatComponent>(entity_id);
		});

	lua.set_function("World_GrantExperience", [this](int player_id, int amount, sol::optional<std::string> source) {
		std::string src = source.value_or(std::string("unknown"));
		GrantExperience(player_id, amount, src);
		});

	lua.set_function("World_AcceptQuest", [this](int player_id, const std::string& quest_id) {
		AcceptQuest(player_id, quest_id);
		});

	lua.set_function("World_ProgressQuest", [this](int player_id, const std::string& quest_id, const std::string& objective_id, int delta) {
		ProgressQuest(player_id, quest_id, objective_id, delta);
		});

	lua.set_function("World_CompleteQuest", [this](int player_id, const std::string& quest_id) {
		CompleteQuest(player_id, quest_id);
		});

	lua.set_function("World_IsQuestActive", [this](int player_id, const std::string& quest_id) -> bool {
		return IsQuestActive(player_id, quest_id);
		});

	lua.set_function("get_meta", [this](int entity_id) -> sol::table {
		return JsonToLua(lua, GetMetaForLua(entity_id));
		});

	lua.set_function("World_GetFactionStanding", [this](int player_id, const std::string& faction_id) -> int {
		return GetFactionStanding(player_id, faction_id);
		});

	lua.set_function("World_AdjustFactionStanding", [this](int player_id, const std::string& faction_id, int delta) -> int {
		return AdjustFactionStanding(player_id, faction_id, delta);
		});

	lua.set_function("World_GetGold", [this](int player_id, sol::optional<std::string> slot) -> int {
		return GetGold(player_id, slot.value_or(std::string("")));
		});

	lua.set_function("World_AddGold", [this](int player_id, int amount, sol::optional<std::string> slot) -> int {
		return AddGold(player_id, amount, slot.value_or(std::string("")));
		});

	lua.set_function("World_QuoteBuyPrice", [this](const std::string& keeper_id, const std::string& template_id) -> int {
		return QuoteBuyPrice(keeper_id, template_id);
		});

	lua.set_function("World_QuoteSellPrice", [this](const std::string& keeper_id, const std::string& template_id) -> int {
		return QuoteSellPrice(keeper_id, template_id);
		});

	lua.set_function("World_ShopBuy", [this](int player_id, const std::string& keeper_id, const std::string& template_id, int qty) -> bool {
		return ShopBuy(player_id, keeper_id, template_id, qty);
		});

	lua.set_function("World_ShopSell", [this](int player_id, const std::string& keeper_id, const std::string& template_id, int qty) -> bool {
		return ShopSell(player_id, keeper_id, template_id, qty);
		});
}

void ScriptManager::GrantExperience(int playerID, int amount, const std::string& source) {
	if (amount <= 0) return;

	auto* vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) {
		registry.AddComponent<PlayerVariablesComponent>(playerID);
		vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
		if (!vars) return;
	}

	int currentXp = vars->intVars["xp"];
	int currentLevel = vars->intVars["level"];
	if (currentLevel <= 0) currentLevel = 1;

	int newXp = currentXp + amount;

	auto xpForLevel = [](int level) -> int {
		return 100 * level;
	};

	int newLevel = currentLevel;
	while (newLevel < 100 && newXp >= xpForLevel(newLevel)) {
		newXp -= xpForLevel(newLevel);
		++newLevel;
	}

	vars->intVars["xp"] = newXp;
	vars->intVars["level"] = newLevel;

	if (gameContext && gameContext->eventBus) {
		EventContext ctx;
		ctx.data = XpGainEventData{ playerID, amount, source };
		gameContext->eventBus->Publish(EventType::XpGain, ctx);

		if (newLevel > currentLevel) {
			EventContext lvlCtx;
			lvlCtx.data = LevelUpEventData{ playerID, newLevel };
			gameContext->eventBus->Publish(EventType::LevelUp, lvlCtx);
		}
	}
}

namespace {

struct QuestProgress {
	std::map<std::string, int> jsonObjective;
};

bool ParseQuestProgress(const std::string& jsonStr, QuestProgress& out) {
	if (jsonStr.empty()) return true;
	try {
		auto j = nlohmann::json::parse(jsonStr);
		if (j.contains("objectives") && j["objectives"].is_object()) {
			for (auto& [k, v] : j["objectives"].items()) {
				out.jsonObjective[k] = v.get<int>();
			}
		}
		return true;
	} catch (...) {
		return false;
	}
}

std::string SerializeQuestProgress(const QuestProgress& qp) {
	nlohmann::json j;
	j["objectives"] = nlohmann::json::object();
	for (auto& [k, v] : qp.jsonObjective) {
		j["objectives"][k] = v;
	}
	return j.dump();
}

PlayerVariablesComponent* GetOrCreateVars(Registry& registry, int playerID) {
	auto* vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) {
		registry.AddComponent<PlayerVariablesComponent>(playerID);
		vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
	}
	return vars;
}

}

void ScriptManager::AcceptQuest(int playerID, const std::string& questId) {
	if (questId.empty()) return;
	auto* vars = GetOrCreateVars(registry, playerID);
	if (!vars) return;

	auto it = vars->stringVars.find(questId);
	if (it != vars->stringVars.end()) return;

	QuestProgress qp;
	vars->stringVars[questId] = SerializeQuestProgress(qp);

	if (gameContext && gameContext->eventBus) {
		EventContext ctx;
		ctx.data = QuestEventData{ playerID, questId, "", 0 };
		gameContext->eventBus->Publish(EventType::QuestAccept, ctx);
	}
}

void ScriptManager::ProgressQuest(int playerID, const std::string& questId, const std::string& objectiveId, int delta) {
	if (questId.empty() || objectiveId.empty() || delta <= 0) return;
	auto* vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) return;

	auto it = vars->stringVars.find(questId);
	if (it == vars->stringVars.end()) return;

	QuestProgress qp;
	if (!ParseQuestProgress(it->second, qp)) return;
	qp.jsonObjective[objectiveId] += delta;
	it->second = SerializeQuestProgress(qp);

	if (gameContext && gameContext->eventBus) {
		EventContext ctx;
		ctx.data = QuestEventData{ playerID, questId, objectiveId, qp.jsonObjective[objectiveId] };
		gameContext->eventBus->Publish(EventType::QuestObjectiveProgress, ctx);
	}
}

void ScriptManager::CompleteQuest(int playerID, const std::string& questId) {
	if (questId.empty()) return;
	auto* vars = GetOrCreateVars(registry, playerID);
	if (!vars) return;

	auto it = vars->stringVars.find(questId);
	if (it != vars->stringVars.end() && it->second == "__completed__") return;

	QuestProgress qp;
	vars->stringVars[questId] = "__completed__";

	if (gameContext && gameContext->eventBus) {
		EventContext ctx;
		ctx.data = QuestEventData{ playerID, questId, "", 0 };
		gameContext->eventBus->Publish(EventType::QuestComplete, ctx);
	}
}

bool ScriptManager::IsQuestActive(int playerID, const std::string& questId) const {
	auto* vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) return false;
	auto it = vars->stringVars.find(questId);
	if (it == vars->stringVars.end()) return false;
	return it->second != "__completed__";
}

bool ScriptManager::IsQuestCompleted(int playerID, const std::string& questId) const {
	auto* vars = registry.GetComponent<PlayerVariablesComponent>(playerID);
	if (!vars) return false;
	auto it = vars->stringVars.find(questId);
	if (it == vars->stringVars.end()) return false;
	return it->second == "__completed__";
}

namespace {

}

nlohmann::json ScriptManager::GetMetaForLua(int entityID) const {
	auto* meta = registry.GetComponent<MetaComponent>(entityID);
	if (!meta) return nlohmann::json::object();
	return meta->meta;
}

int ScriptManager::GetFactionStanding(int playerID, const std::string& factionId) {
	if (!gameContext || !gameContext->factories) return 0;
	return gameContext->factories->factions.GetStanding(playerID, factionId);
}

int ScriptManager::AdjustFactionStanding(int playerID, const std::string& factionId, int delta) {
	if (!gameContext || !gameContext->factories) return 0;
	return gameContext->factories->factions.AdjustStanding(playerID, factionId, delta);
}

int ScriptManager::GetGold(int playerID, const std::string& slot) {
	if (!gameContext || !gameContext->factories) return 0;
	return gameContext->factories->shops.GetPlayerGold(playerID, slot);
}

int ScriptManager::AddGold(int playerID, int amount, const std::string& slot) {
	if (!gameContext || !gameContext->factories) return GetGold(playerID, slot);
	std::string key = slot.empty() ? "gold" : slot;
	int current = gameContext->factories->shops.GetPlayerGold(playerID, key);
	int next = std::max<int>(0, current + amount);
	gameContext->factories->shops.SetGold(playerID, next, key);
	return next;
}

int ScriptManager::QuoteBuyPrice(const std::string& keeperId, const std::string& templateId) const {
	if (!gameContext || !gameContext->factories) return -1;
	return gameContext->factories->shops.QuoteBuyPrice(keeperId, templateId);
}

int ScriptManager::QuoteSellPrice(const std::string& keeperId, const std::string& templateId) const {
	if (!gameContext || !gameContext->factories) return -1;
	return gameContext->factories->shops.QuoteSellPrice(keeperId, templateId);
}

bool ScriptManager::ShopBuy(int playerID, const std::string& keeperId, const std::string& templateId, int qty) {
	if (!gameContext || !gameContext->factories) return false;
	return gameContext->factories->shops.Buy(playerID, keeperId, templateId, qty);
}

bool ScriptManager::ShopSell(int playerID, const std::string& keeperId, const std::string& templateId, int qty) {
	if (!gameContext || !gameContext->factories) return false;
	return gameContext->factories->shops.Sell(playerID, keeperId, templateId, qty);
}

template<typename... Args>
void ScriptManager::BroadcastEvent(const std::string& eventName, Args&&... args) {
	if (lua["Events"][eventName].valid()) {
		lua["Events"][eventName](std::forward<Args>(args)...);
	}
}

void ScriptManager::dispatch_event(const std::string& event_name, sol::table data) {
	if (event_listeners.count(event_name)) {
		for (auto& func : event_listeners[event_name]) {
			auto result = func(data);
			if (!result.valid()) {
				sol::error err = result;
				std::cerr << "Lua Event Error [" << event_name << "]: " << err.what() << std::endl;
			}
		}
	}
}

SkillResult ScriptManager::ExecuteSkillScript(const std::string& scriptPath, const SkillContext& ctx) {
	// 1. Load Script
	//sol::load_result script = lua.load_file(scriptPath);
	// Get Scripts Table

	//if (!script.valid()) {
	//	std::cerr << "[LUA ERROR] Load failed: " << scriptPath << std::endl;
	//	return SkillResult{ false };
	//}
	sol::protected_function func = lua["skills"][scriptPath]["on_execute"];
	// 2. Initialize Script (Runs global scope)

	// 3. Find Function
	//sol::protected_function func = lua["on_execute"];
	if (!func.valid()) {
		std::cerr << "[LUA ERROR] No 'on_execute' in " << scriptPath << std::endl;
		return SkillResult{ false };
	}

	// 4. Execute with Context
	// Get the skill table to pass as 'self' parameter
	sol::table skillTable = lua["skills"][scriptPath];
	
	// Convert C++ SkillContext to Lua table
	sol::table ctxTable = lua.create_table();
	ctxTable["sourceID"] = ctx.sourceID;
	ctxTable["targetID"] = ctx.targetID;
	ctxTable["skillID"] = ctx.skillID;
	ctxTable["masteryLevel"] = ctx.masteryLevel;
	ctxTable["basePower"] = ctx.basePower;

	
	auto result = func(skillTable, ctxTable);

	// 5. Debug & Unpack Result
	std::cerr << "[DEBUG] Result valid: " << result.valid() << std::endl;
	std::cerr << "[DEBUG] Return count: " << result.return_count() << std::endl;
	
	if (!result.valid()) {
		sol::error err = result;
		std::cerr << "[LUA ERROR] Skill execution failed: " << err.what() << std::endl;
		return SkillResult{ false };
	}
	
	if (result.return_count() > 0) {
		sol::object firstReturn = result[0];
		std::cerr << "[DEBUG] First return type (int): " << static_cast<int>(firstReturn.get_type()) << std::endl;
		std::cerr << "[DEBUG] Is table: " << firstReturn.is<sol::table>() << std::endl;
		
		if (firstReturn.is<sol::table>()) {
			sol::table tbl = firstReturn;
			
			// Debug: print all keys in the table
			std::cerr << "[DEBUG] Table keys:" << std::endl;
			for (auto& pair : tbl) {
				if (pair.first.is<std::string>()) {
					std::cerr << "  - " << pair.first.as<std::string>() << std::endl;
				}
			}
			
			SkillResult res;
			res.success = tbl.get_or("success", false);
			res.actionType = tbl.get_or<std::string>("actionType", "none");
			res.magnitude = tbl.get_or<float>("magnitude", 0.0);
			res.damageType = tbl.get_or<std::string>("damageType", "physical");
			res.dataString = tbl.get_or<std::string>("dataString", "attacked");
			
			sol::object tagsObj = tbl["addedTags"];
			if (tagsObj.is<sol::table>()) {
				sol::table tagsTbl = tagsObj;
				for (auto& pair : tagsTbl) {
					if (pair.second.is<std::string>()) {
						res.addedTags.push_back(pair.second.as<std::string>());
					}
				}
			}
			return res;
		}
	}

	return SkillResult{ false };
}

void ScriptManager::load_script(const std::string& path) {
	auto loaded = lua.load_file(path);
	if (!loaded.valid()) {
		sol::error err = loaded;
		std::cerr << "Failed to parse " << path << ": " << err.what() << std::endl;
		return;
	}
	auto result = loaded();
	if (!result.valid()) {
		sol::error err = result;
		std::cerr << "Failed to execute " << path << ": " << err.what() << std::endl;
	}
}

void ScriptManager::load_all_scripts(const std::string& root_path) {
	try {
		if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
			std::cerr << "Script directory not found: " << root_path << std::endl;
			return;
		}

		for (const auto& entry : fs::recursive_directory_iterator(root_path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".lua") {
				std::string path = entry.path().string();
				std::cout << "Loading script: " << path << std::endl;
				load_script(path);
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error scanning scripts: " << e.what() << std::endl;
	}
}

InteractableResult ScriptManager::ExecuteInteractableScript(const std::string& scriptPath, const std::string& functionName, const InteractableContext& context) {
	// 1. Load Script
	sol::load_result script = lua.load_file(scriptPath);
	if (!script.valid()) {
		std::cerr << "[LUA ERROR] Load failed: " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// 2. Initialize Script (Runs global scope)
	sol::protected_function_result scriptBody = script();
	if (!scriptBody.valid()) {
		std::cerr << "[LUA ERROR] Exec failed: " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// 3. Find Function
	sol::protected_function func = lua[functionName];
	if (!func.valid()) {
		std::cerr << "[LUA ERROR] No '" << functionName << "' in " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// 4. Execute with Context
	auto result = func(context);

	// 5. Unpack Result
	if (result.valid() && result.return_count() > 0 && result[0].is<sol::table>()) {
		sol::table tbl = result[0];
		InteractableResult res;

		res.success = tbl.get_or("success", false);
		res.actionType = tbl.get_or<std::string>("actionType", "none");
		res.message = tbl.get_or<std::string>("message", "");
		res.roomMessage = tbl.get_or<std::string>("roomMessage", "");
		
		// Teleport data
		res.targetRoomID = tbl.get_or("targetRoomID", -1);
		res.targetX = tbl.get_or("targetX", -1);
		res.targetY = tbl.get_or("targetY", -1);
		
		// Item spawning
		res.spawnItemID = tbl.get_or<std::string>("spawnItemID", "");
		res.spawnX = tbl.get_or("spawnX", -1);
		res.spawnY = tbl.get_or("spawnY", -1);
		
		// Event triggering
		res.eventName = tbl.get_or<std::string>("eventName", "");
		res.newState = tbl.get_or<std::string>("newState", "");
		res.consumeOnUse = tbl.get_or("consumeOnUse", false);

		// Parse event parameters array
		sol::object paramsObj = tbl["eventParams"];
		if (paramsObj.is<sol::table>()) {
			sol::table paramsTbl = paramsObj;
			for (auto& pair : paramsTbl) {
				if (pair.second.is<std::string>()) {
					res.eventParams.push_back(pair.second.as<std::string>());
				}
			}
		}

		return res;
	}

	return InteractableResult{ false };
}

ClientConnection* ScriptManager::GetPlayer(int player_id) {
	auto* comp = registry.GetComponent<ClientComponent>(player_id);
	return comp ? comp->client : nullptr;
}
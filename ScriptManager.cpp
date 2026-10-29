#include "ScriptManager.h"
#include "Registry.h"
#include "Component.h"
#include "ClientConnection.h"
#include "InteractableContext.h"
#include "SkillContext.h"
#include "EventBus.h"
#include "GameContext.h"
#include "FactionFactory.h"
#include "ShopFactory.h"
#include "FactoryManager.h"
#include "ItemScriptContext.h"
#include "ItemScriptResult.h"
#include "Platform.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "sol/sol.hpp"

namespace fs = std::filesystem;

void CreateMagicBindings(const std::string& version, PlayerVariablesComponent* vars);

std::string ScriptManager::DiscoverScriptsRoot() {
	// 1. Check env var override
	if (const char* env = std::getenv("MUD_SCRIPTS_PATH"); env && *env) {
		fs::path p(env);
		if (fs::is_directory(p)) {
			std::error_code ec;
			return fs::absolute(p, ec).string();
		}
	}

	// 2. Walk up from the executable's directory
	fs::path exeDir;
#ifdef PLATFORM_WINDOWS
	char buffer[MAX_PATH];
	DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	if (len > 0 && len < MAX_PATH) {
		buffer[len] = '\0';
		exeDir = fs::path(buffer).parent_path();
	}
#else
	char buffer[4096];
	ssize_t rlen = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (rlen > 0) {
		buffer[rlen] = '\0';
		exeDir = fs::path(buffer).parent_path();
	}
#endif

	for (int i = 0; i < 6; ++i) {
		if (exeDir.empty()) break;
		fs::path candidate = exeDir / "scripts";
		if (fs::is_directory(candidate)) {
			std::error_code ec;
			return fs::absolute(candidate, ec).string();
		}
		if (!exeDir.has_parent_path()) break;
		exeDir = exeDir.parent_path();
	}

	// 3. Fall back to CWD-relative
	fs::path cwdScripts = fs::current_path() / "scripts";
	if (fs::is_directory(cwdScripts)) {
		std::error_code ec;
		return fs::absolute(cwdScripts, ec).string();
	}

	return "";
}

std::string ScriptManager::ResolveScriptPath(const std::string& path, const std::string& kindDir) const {
	if (path.empty()) return "";

	fs::path p(path);

	// If the path has a directory separator, treat as relative to scriptsRoot or absolute.
	if (path.find('/') != std::string::npos || path.find('\\') != std::string::npos) {
		if (p.is_absolute()) return p.string();
		return (fs::path(scriptsRoot) / p).string();
	}

	// Bare name. Append .lua if missing.
	std::string withExt = path;
	if (withExt.size() < 4 || withExt.substr(withExt.size() - 4) != ".lua") {
		withExt += ".lua";
	}

	// Try scriptsRoot/<kindDir>/<name>.lua
	if (!kindDir.empty()) {
		fs::path candidate = fs::path(scriptsRoot) / kindDir / withExt;
		if (fs::exists(candidate)) return candidate.string();
	}

	// Fallback: scriptsRoot/<name>.lua (in case the user gave the path with dir)
	fs::path candidate = fs::path(scriptsRoot) / withExt;
	if (fs::exists(candidate)) return candidate.string();

	return "";
}

sol::protected_function ScriptManager::LoadOrGetScript(const std::string& path, const std::string& kindDir, const std::string& functionName) {
	std::string resolved = ResolveScriptPath(path, kindDir);
	if (resolved.empty()) {
		std::cerr << "[ScriptManager] Script not found: '" << path << "' (kind=" << kindDir << ")" << std::endl;
		if (!scriptsRoot.empty()) {
			std::cerr << "[ScriptManager]   scriptsRoot = " << scriptsRoot << std::endl;
		}
		return sol::protected_function();
	}

	// Always (re)load. The dofile chain in master files already loads
	// these at boot, and the global scope is idempotent for our scripts.
	// Re-running it per invocation is the hot-reload mechanism.
	sol::load_result loaded = lua.load_file(resolved);
	if (!loaded.valid()) {
		sol::error err = loaded;
		std::cerr << "[ScriptManager] Load failed: " << resolved << " : " << err.what() << std::endl;
		return sol::protected_function();
	}

	sol::protected_function_result exec = loaded();
	if (!exec.valid()) {
		sol::error err = exec;
		std::cerr << "[ScriptManager] Exec failed: " << resolved << " : " << err.what() << std::endl;
		return sol::protected_function();
	}

	return lua[functionName];
}

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
		}
		else if (val.is_boolean()) {
			tbl[key] = val.get<bool>();
		}
		else if (val.is_number_integer()) {
			tbl[key] = val.get<int>();
		}
		else if (val.is_number_float()) {
			tbl[key] = val.get<double>();
		}
		else if (val.is_null()) {
			tbl[key] = sol::nil;
		}
		else if (val.is_object()) {
			tbl[key] = JsonToLua(lua, val);
		}
		else {
			tbl[key] = sol::nil;
		}
	}
	return tbl;
}


ScriptManager::ScriptManager(Registry& r) : registry(r) {
	// Initialize lua state in constructor
	lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::string);

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

	lua.new_usertype<ItemScriptContext>("ItemScriptContext",
		"userID", &ItemScriptContext::userID,
		"itemID", &ItemScriptContext::itemID,
		"slot", &ItemScriptContext::slot,
		"itemTemplateID", &ItemScriptContext::itemTemplateID,
		"roomID", &ItemScriptContext::roomID,
		"x", &ItemScriptContext::x,
		"y", &ItemScriptContext::y
	);

	//init();
}

void ScriptManager::init() {
	scriptsRoot = DiscoverScriptsRoot();
	if (scriptsRoot.empty()) {
		std::cerr << "[ScriptManager] FATAL: scripts/ directory not found. Set MUD_SCRIPTS_PATH or run from a directory that has a reachable scripts/ folder." << std::endl;
	} else {
		std::cout << "[ScriptManager] Scripts root: " << scriptsRoot << std::endl;
		// Make package.path point at the absolute scripts root so that
		// dofile() chains inside master files (which use CWD-relative
		// paths like "scripts/skills/alchemy/calcify.lua") work regardless
		// of the binary's CWD.
		std::string newPath = scriptsRoot + "/?.lua";
		sol::object existing = lua["package"]["path"];
		if (existing.is<std::string>()) {
			newPath += ";";
			newPath += existing.as<std::string>();
		}
		lua["package"]["path"] = newPath;
	}

	// Load the interactable scripts master
	{
		std::string p = ResolveScriptPath("interactables_master", "interactables");
		if (!p.empty()) {
			sol::load_result loaded = lua.load_file(p);
			if (!loaded.valid()) {
				sol::error err = loaded;
				std::cerr << "Failed to parse " << p << ": " << err.what() << std::endl;
			} else {
				auto result = loaded();
				if (!result.valid()) {
					sol::error err = result;
					std::cerr << "Failed to execute " << p << ": " << err.what() << std::endl;
				}
			}
		} else {
			std::cerr << "[ScriptManager] interactables_master.lua not found." << std::endl;
		}
	}

	// Load the skills scripts master
	{
		std::string p = ResolveScriptPath("skills_master", "skills");
		if (!p.empty()) {
			sol::load_result loaded = lua.load_file(p);
			if (!loaded.valid()) {
				sol::error err = loaded;
				std::cerr << "Failed to parse " << p << ": " << err.what() << std::endl;
			} else {
				auto result = loaded();
				if (!result.valid()) {
					sol::error err = result;
					std::cerr << "Failed to execute " << p << ": " << err.what() << std::endl;
				}
			}
		} else {
			std::cerr << "[ScriptManager] skills_master.lua not found." << std::endl;
		}
	}

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
		registry.AddComponent<PlayerDirtyComponent>(entity_id);
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
	lua.set_function("magic_get_known", [this](int player_id)-> sol::table {
		return GetMagicKnown(player_id);
		});
	lua.set_function("learn_magic", [this](int player_id, std::string magic_code)-> sol::table {
		return LearnMagic(player_id, magic_code);
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
		}
		catch (...) {
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
sol::table ScriptManager::GetMagicKnown(int playerID) {
	auto* vars = GetOrCreateVars(registry, playerID);
	if (!vars) return lua.create_table();

	if (vars->stringVars.find("magic") == vars->stringVars.end()) CreateMagicBindings("v1", vars);

	try {
		return JsonToLua(lua, nlohmann::json::parse(vars->stringVars["magic"]));
	}
	catch (...) {
		return lua.create_table();
	}
}

sol::table ScriptManager::LearnMagic(int playerID, const std::string& magicCode) {
	auto* vars = GetOrCreateVars(registry, playerID);
	if (!vars) return lua.create_table();

	if (vars->stringVars.find("magic") == vars->stringVars.end()) CreateMagicBindings("v1", vars);

	nlohmann::json magic;
	try {
		magic = nlohmann::json::parse(vars->stringVars["magic"]);
	}
	catch (...) {
		CreateMagicBindings("v1", vars);
		magic = nlohmann::json::parse(vars->stringVars["magic"]);
	}

	if (!magic.contains("known") || !magic["known"].is_object()) {
		magic["known"] = nlohmann::json::object();
	}
	magic["known"][magicCode] = true;
	vars->stringVars["magic"] = magic.dump();

	return JsonToLua(lua, magic);
}

void CreateMagicBindings(const std::string& version, PlayerVariablesComponent* vars) {
	if (version == "v1") {
		vars->stringVars["magic"] = R"({"v":1,"trees":{},"known":{},"slots":{}})";
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
	auto* vars = GetOrCreateVars(registry, playerID);
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
	(void)root_path; // legacy parameter; resolver finds scriptsRoot on its own
	try {
		std::string path = ResolveScriptPath("master_file", "");
		if (!path.empty()) {
			std::cout << "Loading script: " << path << std::endl;
			load_script(path);
		} else {
			std::cerr << "Script not found: master_file.lua (searched under scripts root)" << std::endl;
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error scanning scripts: " << e.what() << std::endl;
	}
}

InteractableResult ScriptManager::ExecuteInteractableScript(const std::string& scriptPath, const std::string& functionName, const InteractableContext& context) {
	sol::protected_function func = LoadOrGetScript(scriptPath, "interactables", functionName);
	if (!func.valid()) {
		std::cerr << "[LUA ERROR] No '" << functionName << "' for script: " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// Execute with Context
	auto result = func(context);

	// Unpack Result
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

ItemScriptResult ScriptManager::ExecuteItemScript(const std::string& scriptPath, const std::string& functionName, const ItemScriptContext& context) {
	sol::protected_function func = LoadOrGetScript(scriptPath, "items", functionName);
	if (!func.valid()) {
		std::cerr << "[LUA ERROR] No '" << functionName << "' for item script: " << scriptPath << std::endl;
		return ItemScriptResult{ false };
	}

	auto result = func(context);

	if (result.valid() && result.return_count() > 0 && result[0].is<sol::table>()) {
		sol::table tbl = result[0];
		ItemScriptResult res;
		res.success = tbl.get_or("success", true);
		res.cancelEquip = tbl.get_or("cancelEquip", false);
		res.cancelUnequip = tbl.get_or("cancelUnequip", false);
		res.cancelDrop = tbl.get_or("cancelDrop", false);
		res.message = tbl.get_or<std::string>("message", "");
		res.roomMessage = tbl.get_or<std::string>("roomMessage", "");
		return res;
	}

	return ItemScriptResult{ false };
}
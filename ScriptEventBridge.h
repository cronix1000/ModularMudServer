#pragma once
#include "EventBus.h"
#include "ScriptManager.h"
class ScriptEventBridge
{
	EventBus* bus;
    ScriptManager* scripts;
public:
    ScriptEventBridge(EventBus* b, ScriptManager* s) : bus(b), scripts(s) {
        // --- BRIDGE: ROOM ENTERED ---
        bus->Subscribe(EventType::RoomEntered, [this](const EventContext& ctx) {
            auto data = std::get<RoomEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["entity_id"] = data.EntityID;
            luaTable["room_id"] = data.RoomID;

            scripts->dispatch_event("RoomEntered", luaTable);
            });
        bus->Subscribe(EventType::CombatHit, [this](const EventContext& ctx) {
            auto data = std::get<CombatEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["attacker_id"] = data.attackerID;
            luaTable["victim_id"] = data.victimID;

            scripts->dispatch_event("CombatHit", luaTable);
            });
        bus->Subscribe(EventType::XpGain, [this](const EventContext& ctx) {
            auto data = std::get<XpGainEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["player_id"] = data.playerID;
            luaTable["amount"] = data.amount;
            luaTable["source"] = data.source;

            scripts->dispatch_event("XpGain", luaTable);
            });
        bus->Subscribe(EventType::LevelUp, [this](const EventContext& ctx) {
            auto data = std::get<LevelUpEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["player_id"] = data.playerID;
            luaTable["new_level"] = data.newLevel;

            scripts->dispatch_event("LevelUp", luaTable);
            });
        bus->Subscribe(EventType::QuestAccept, [this](const EventContext& ctx) {
            auto data = std::get<QuestEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["player_id"] = data.playerID;
            luaTable["quest_id"] = data.questId;

            scripts->dispatch_event("QuestAccept", luaTable);
            });
        bus->Subscribe(EventType::QuestObjectiveProgress, [this](const EventContext& ctx) {
            auto data = std::get<QuestEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["player_id"] = data.playerID;
            luaTable["quest_id"] = data.questId;
            luaTable["objective_id"] = data.objectiveId;
            luaTable["count"] = data.count;

            scripts->dispatch_event("QuestObjectiveProgress", luaTable);
            });
        bus->Subscribe(EventType::QuestComplete, [this](const EventContext& ctx) {
            auto data = std::get<QuestEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["player_id"] = data.playerID;
            luaTable["quest_id"] = data.questId;

            scripts->dispatch_event("QuestComplete", luaTable);
            });
        bus->Subscribe(EventType::FactionChange, [this](const EventContext& ctx) {
            auto data = std::get<FactionChangeEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["player_id"] = data.playerID;
            luaTable["faction_id"] = data.factionId;
            luaTable["old_standing"] = data.oldStanding;
            luaTable["new_standing"] = data.newStanding;

            scripts->dispatch_event("FactionChange", luaTable);
            });
    }
    ~ScriptEventBridge() = default;

private:

};

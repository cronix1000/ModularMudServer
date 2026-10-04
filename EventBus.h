#pragma once
#include <vector>
#include <map>
#include <functional>
#include <string>

enum class EventType {
    None = 0,
    RoomEntered,   // <--- This is the one you need
    EntityMoved,
    PlayerJoined,
    CombatHit,
    ItemEquipped,
    XpGain,
    LevelUp,
    QuestAccept,
    QuestObjectiveProgress,
    QuestComplete,
    FactionChange
};
#include <variant>
#include <queue>

struct RoomEventData {
    int EntityID;
    int RoomID;
};

// Data specific to fighting
struct CombatEventData {
    int attackerID;
    int victimID;
};

// Data specific to chatting
struct ChatEventData {
    int senderID;
    std::string message;
};

struct PlayerEventData {
    int player;
    std::string name;
};

struct ItemEquippedEventData {
    int player;
    int itemId;
};

struct PlayerLoggedInData {
    int playerID;
    std::string playerName;
    int permissionLevel;
};

struct XpGainEventData {
    int playerID;
    int amount;
    std::string source;
};

struct LevelUpEventData {
    int playerID;
    int newLevel;
};

struct QuestEventData {
    int playerID;
    std::string questId;
    std::string objectiveId;
    int count;
};

struct FactionChangeEventData {
    int playerID;
    std::string factionId;
    int oldStanding;
    int newStanding;
};

struct EventContext {
    std::variant<std::monostate, RoomEventData, CombatEventData, ChatEventData, PlayerEventData, ItemEquippedEventData, PlayerLoggedInData, XpGainEventData, LevelUpEventData, QuestEventData, FactionChangeEventData> data;
};

class EventBus {
    // Define a "Callback" as a function that takes context and returns void
    using Callback = std::function<void(const EventContext&)>;

    // A map of: EventType -> List of Listeners
    std::map<EventType, std::vector<Callback>> subscribers;
    std::queue<std::pair<Callback, EventContext>> deferredCalls;

public:
    // Systems "Subscribe" to listen for events
    void Subscribe(EventType type, Callback callback) {
        subscribers[type].push_back(callback);
    }

    // Systems "Publish" events when things happen
    void Publish(EventType type, const EventContext& ctx, bool deferred = false) {
        if (subscribers.find(type) == subscribers.end()) return;

        auto check = subscribers.find(type);
        if (check == subscribers.end()) {
            return;
        }

        for (auto& callback : subscribers[type]) {
            if (deferred) {
                // Store it for later
                deferredCalls.push({ callback, ctx });
            }
            else {
                callback(ctx);
            }
        }
    }
    // delayed events that get called at the end of the tick
    void CallDefferedCalls() {
        while (!deferredCalls.empty()) {
            auto& [callback, context] = deferredCalls.front();

            callback(context);

            deferredCalls.pop();
        }
    }
};
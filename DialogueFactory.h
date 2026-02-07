#pragma once
#include "GameContext.h"
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include "DialogueComponent.h"
#include "VoiceComponent.h"

using json = nlohmann::json;

struct DialogueOption {
    std::string text;
    std::string targetNode;
    std::string luaCallback;
};

struct DialogueNode {
    std::string text;
    std::vector<DialogueOption> options;
};

struct VoiceSet {
    std::vector<std::string> idleLines;
    std::vector<std::string> combatLines;
    std::vector<std::string> deathLines;
};

class DialogueFactory {
public:
    GameContext& ctx;
    std::map<std::string, DialogueNode> dialogueNodes; // For interactive NPCs
    std::map<std::string, VoiceSet> voiceLibrary;      // For random Mobs

    DialogueFactory(GameContext& g) : ctx(g) {}

    void LoadDialogueAndVoices(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        json data;
        file >> data;

        for (auto& [key, val] : data.items()) {
            // TYPE 1: Interactive Dialogue Node
            if (val.contains("text")) {
                DialogueNode node;
                node.text = val.value("text", "...");

                if (val.contains("options") && val["options"].is_array()) {
                    for (auto& optJson : val["options"]) {
                        node.options.push_back({
                            optJson.value("text", "Continue..."),
                            optJson.value("targetNode", "EXIT"),
                            optJson.value("luaCallback", "")
                            });
                    }
                }
                else {
                    node.options.push_back({ "Close", "EXIT", "" });
                }
                dialogueNodes[key] = node;
            }
            // TYPE 2: Simple Voice Barks
            else if (val.contains("idle") || val.contains("combat")) {
                VoiceSet vs;
                vs.idleLines = val.value("idle", std::vector<std::string>{});
                vs.combatLines = val.value("combat", std::vector<std::string>{});
                vs.deathLines = val.value("death", std::vector<std::string>{});
                voiceLibrary[key] = vs;
            }
        }
    }

    // Public Helpers to attach to entities
    void AttachDialogue(EntityID id, const std::string& startNodeID) {
        if (dialogueNodes.find(startNodeID) != dialogueNodes.end()) {
            ctx.registry.AddComponent<DialogueComponent>(id, DialogueComponent{ startNodeID });
        }
    }

    void AttachVoice(EntityID id, const std::string& voiceSetID) {
        auto it = voiceLibrary.find(voiceSetID);
        if (it != voiceLibrary.end()) {
            ctx.registry.AddComponent<VoiceComponent>(id, VoiceComponent{
                it->second.idleLines,
                it->second.combatLines,
                it->second.deathLines
                });
        }
    }
};
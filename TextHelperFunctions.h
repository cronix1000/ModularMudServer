#pragma once
#include <string>
#include <regex>
#include "EquipmentSlot.h"
#include "Direction.h"
#include <map>
 class TextHelperFunctions {
public:
      inline static const std::map<std::string, Direction> directionMapString = {
{"north", Direction::North },
{"south", Direction::South },
{"east", Direction::East },
{"west", Direction::West },
{"up", Direction::North },
{"down", Direction::Down},
{"none", Direction::None },
{"down", Direction::Down}
    };

    inline  static const std::map<Direction, std::string > directionMapDirection = {
    {Direction::North ,"north" },
    {Direction::South ,"south" },
    {Direction::East, "east" },
    {Direction::West, "west" },
    {Direction::North,"up"  },
    {Direction::Down,"down" },
    {Direction::None,"none" },
    {Direction::Down,"down" }
    };

	static std::string Colorize(std::string text) {
        // 1. Define ANSI Colors
        std::string result;
        result.reserve(text.size()); // Pre-allocate memory for speed

        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '&' && i + 1 < text.size()) {
                char code = text[++i]; // Peek at the next char and skip it
                switch (code) {
                case 'x': result += "\033[30m"; break; // Black
                case 'g': result += "\033[32m"; break; // Green
                case 'G': result += "\033[37m"; break; // Gray
                case 'b': result += "\033[34m"; break; // Blue
                case 'y': result += "\033[33m"; break; // Yellow
                case 'r': result += "\033[31m"; break; // Red
                case 'm': result += "\033[35m"; break; // Magenta
                case 'w': result += "\033[0m";  break; // Reset
                default:  result += '&'; result += code; break; // Not a code, keep it
                }
            }
            else {
                result += text[i];
            }
        }
        return result;
    }
    static Direction StringToDirection(std::string dir) {

        Direction direction;
        try {
            direction = directionMapString.at(dir);
        }
        catch (const std::out_of_range&) {
            direction = Direction::None;
        }

        return direction;
    }

    static std::string DirectionToString(Direction dir) {

        std::string direction;
        try {
            direction = directionMapDirection.at(dir);
        }
        catch (const std::out_of_range&) {
            direction = 'none';
        }
        return direction;
    }

static std::string SlotToString(EquipmentSlot slot) {
    switch (slot) {
    case EquipmentSlot::mainArm: return "Main Hand";
    case EquipmentSlot::offHand:  return "Off Hand";
    case EquipmentSlot::chest:   return "Chest";
        case EquipmentSlot::helmet:    return "Head";
        case EquipmentSlot::bracers:    return "Bracers";
        case EquipmentSlot::leggings:    return "Leggings";
    default: return "General";
    }
}


static EquipmentSlot StringToSlot(std::string slot) {

    ToLower(slot);
        if (slot == "main hand" || slot == "main") {
            return EquipmentSlot::mainArm;
        }if (slot == "off hand" || slot == "off") {
            return EquipmentSlot::offHand;
        }if (slot == "chest") {
            return EquipmentSlot::chest;
        }if (slot == "helmet") {
            return EquipmentSlot::helmet;
        }if (slot == "bracers") {
            return EquipmentSlot::bracers;
        }if (slot == "leggings") {
            return EquipmentSlot::leggings;
        }
}


static void ToLower(std::string& text)
{
     std::transform(text.begin(), text.end(), text.begin(), ::tolower);
}

};
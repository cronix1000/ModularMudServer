#pragma once
#include <string>
#include <regex>
#include "InventoryComponents.h"
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
            if (text[i] == '{' && i + 1 < text.size()) {
                char code = text[++i];
                switch (code) {
                case 'x': result += "\033[40m"; break;  // Black bg
                case 'X': result += "\033[100m"; break; // Bright Black bg
                case 'g': result += "\033[42m"; break;  // Green bg
                case 'G': result += "\033[102m"; break; // Bright Green bg
                case 'b': result += "\033[44m"; break;  // Blue bg
                case 'B': result += "\033[104m"; break; // Bright Blue bg
                case 'y': result += "\033[43m"; break;  // Yellow bg
                case 'Y': result += "\033[103m"; break; // Bright Yellow bg
                case 'r': result += "\033[41m"; break;  // Red bg
                case 'R': result += "\033[101m"; break; // Bright Red bg
                case 'm': result += "\033[45m"; break;  // Magenta bg
                case 'M': result += "\033[105m"; break; // Bright Magenta bg
                case 'c': result += "\033[46m"; break;  // Cyan bg
                case 'C': result += "\033[106m"; break; // Bright Cyan bg
                case 'w': result += "\033[47m"; break;  // White bg
                case 'W': result += "\033[107m"; break; // Bright White bg
                case 'n': result += "\033[49m"; break;  // Default bg
                default:  result += '{'; result += code; break; // Not a code, keep it
                }
            }
            else if (text[i] == '&' && i + 1 < text.size()) {
                char code = text[++i]; // Peek at the next char and skip it
                switch (code) {
                case 'x': result += "\033[30m"; break; // Black
                case 'X': result += "\033[90m"; break; // Bright Black
                case 'g': result += "\033[32m"; break; // Green
                case 'G': result += "\033[92m"; break; // Bright Green
                case 'b': result += "\033[34m"; break; // Blue
                case 'B': result += "\033[94m"; break; // Bright Blue
                case 'y': result += "\033[33m"; break; // Yellow
                case 'Y': result += "\033[93m"; break; // Bright Yellow
                case 'r': result += "\033[31m"; break; // Red
                case 'R': result += "\033[91m"; break; // Bright Red
                case 'm': result += "\033[35m"; break; // Magenta
                case 'M': result += "\033[95m"; break; // Bright Magenta
                case 'c': result += "\033[36m";  break; // Cyan
                case 'C': result += "\033[96m"; break; // Bright Cyan
                case 'w': result += "\033[0m";  break; // Reset
                case 'W': result += "\033[97m"; break; // Bright White
                case 'd': result += "\033[29m"; break; // Dark Gray
                case 'D': result += "\033[90m"; break; // Dark Gray (alias)
                case 'n': result += "\033[33m"; break; // Brown -> dark yellow
                case 'o': result += "\033[33m"; break; // Orange -> yellow (closest basic)
                case 'p': result += "\033[35m"; break; // Pink -> magenta (closest basic)
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
    return EquipmentSlotInfo::SlotToString(slot);
}


static EquipmentSlot StringToSlot(std::string slot) {
    return EquipmentSlotInfo::ParseSlotString(slot);
}


static void ToLower(std::string& text)
{
     std::transform(text.begin(), text.end(), text.begin(), ::tolower);
}

};
#include "TextHelperFunctions.h"
const std::map<std::string, Direction> directionMapString = {
{"north", Direction::North },
{"south", Direction::South },
{"east", Direction::East },
{"west", Direction::West },
{"up", Direction::North },
{"down", Direction::Down},
{"none", Direction::None },
{"down", Direction::Down}
};

const std::map<Direction, std::string > directionMapDirection = {
{Direction::North ,"north" },
{Direction::South ,"south" },
{Direction::East, "east" },
{Direction::West, "west" },
{Direction::North,"up"  },
{Direction::Down,"down" },
{Direction::None,"none" },
{Direction::Down,"down" }
};
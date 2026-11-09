#pragma once

struct GameContext;

namespace SkillXP {
	void Grant(GameContext& ctx, int playerID, int skillID, int amount);
	void GrantKillBonus(GameContext& ctx, int playerID, int skillID);
}

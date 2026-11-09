#pragma once

class Registry;

namespace PowerCalculator {
	void RecalculateDerivedStats(Registry* registry, int entityID);
	float AttackInterval(Registry* registry, int entityID);
}

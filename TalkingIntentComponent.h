#pragma once
#include <string>
struct TalkingIntentComponent {
	int recieverId;
	int roomId;
	std::string text;
	bool toRoom;
	bool toWorld;
};
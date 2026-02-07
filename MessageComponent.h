#pragma once
#include <string>
struct MessageComponent {
	std::string message;
	enum MessageOutput {
		clientvclient,
		roomclient
	};
};
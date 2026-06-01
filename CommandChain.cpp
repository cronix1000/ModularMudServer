#include <cstring>
#include "CommandChain.h"
#include <sstream>
#include <algorithm>

std::vector<std::vector<std::string>> CommandChain::Parse(const std::string& input) {
	std::vector<std::vector<std::string>> commands;
	std::string remaining = input;
	
	size_t pos = 0;
	while ((pos = remaining.find(DELIMITER)) != std::string::npos) {
		std::string cmd = remaining.substr(0, pos);
		if (!cmd.empty()) {
			std::vector<std::string> words;
			std::stringstream ss(cmd);
			std::string word;
			while (ss >> word) {
				std::transform(word.begin(), word.end(), word.begin(), ::tolower);
				words.push_back(word);
			}
			if (!words.empty()) {
				commands.push_back(words);
			}
		}
		remaining.erase(0, pos + strlen(DELIMITER));
	}
	
	// Last command
	if (!remaining.empty()) {
		std::vector<std::string> words;
		std::stringstream ss(remaining);
		std::string word;
		while (ss >> word) {
			std::transform(word.begin(), word.end(), word.begin(), ::tolower);
			words.push_back(word);
		}
		if (!words.empty()) {
			commands.push_back(words);
		}
	}
	
	return commands;
}

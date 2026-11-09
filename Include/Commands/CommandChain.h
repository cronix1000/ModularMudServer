#pragma once

#include <vector>
#include <string>

class CommandChain {
public:
	static std::vector<std::vector<std::string>> Parse(const std::string& input);
	
private:
	static constexpr const char* DELIMITER = "; ";
};

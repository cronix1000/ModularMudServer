#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>

class Command {
public:
    std::string CommandString;
    std::vector<std::string> Parameters;

    Command() {}
    ~Command() {}
    void ParseInput(const std::string& rawInput);
    void ParseInput(std::vector<std::string> p);
    void Deserialize(char* buffer);
    char* Serialize(int& totalLength);
    void ParseParameters();
    void ToLower();
private:
    const size_t FIXED_NAME_SIZE = 40;
    const size_t FIXED_COMMAND_SIZE = 40;
};

#include "Command.h"
#include <iostream>
#include <string>
#include <algorithm>

void Command::Deserialize(char* buffer) {
    char* currentPtr = buffer;

    // 1. Read Total Length
    int totalLength = *(int*)currentPtr;
    printf("[DEBUG] Total Length: %d\n", totalLength); // CHECK THIS VALUE
    currentPtr += 4;

    // 2. Read Verb Length
    int verbLen = *(int*)currentPtr;
    printf("[DEBUG] Verb Length: %d\n", verbLen);      // CHECK THIS VALUE
    currentPtr += 4;

    // SAFETY CHECK: If verbLen is huge, we are reading garbage
    if (verbLen < 0 || verbLen > 1000) {
        printf("[CRITICAL] Verb length is crazy! Data corruption.\n");
        return;
    }

    // 3. Read Verb String
    CommandString.assign(currentPtr, verbLen);
    printf("[DEBUG] Verb: %s\n", CommandString.c_str());
    currentPtr += verbLen;

    // 4. Read Param Count
    int paramCount = *(int*)currentPtr;
    printf("[DEBUG] Params Found: %d\n", paramCount);
    currentPtr += 4;

    Parameters.clear();
    for (int i = 0; i < paramCount; ++i) {
        int paramLen = *(int*)currentPtr;
        currentPtr += 4;

        std::string p(currentPtr, paramLen);
        printf("[DEBUG] Param %d: %s\n", i, p.c_str());
        Parameters.push_back(p);
        currentPtr += paramLen;
    }
}

void Command::ToLower()
{
    for (std::string param : Parameters) 
    {
        std::transform(param.begin(), param.end(), param.begin(), ::tolower);
    }
    std::transform(CommandString.begin(), CommandString.end(), CommandString.begin(), ::tolower);
}

char* Command::Serialize(int& totalLength) {
    totalLength = 4 + 4 + CommandString.size() + 4; // Headers for TotalLen, VerbLen, ParamCount
    for (const auto& param : Parameters) {
        totalLength += 4; // Add space for each Param's Length
        totalLength += param.size();
    }

    char* buffer = new char[totalLength];
    char* currentPtr = buffer;

    *(int*)currentPtr = totalLength;
    currentPtr += 4;

    // 3. Write Verb Length and String
    int commandStringLen = CommandString.size();
    *(int*)currentPtr = commandStringLen;
    currentPtr += 4;
    memcpy(currentPtr, CommandString.c_str(), commandStringLen);
    currentPtr += commandStringLen;

    // 4. Write Parameter Count
    int paramCount = Parameters.size();
    *(int*)currentPtr = paramCount;
    currentPtr += 4;

    // 5. Write each Parameter Length and String
    for (const auto& param : Parameters) {
        int paramLen = param.size();

        // Write Length
        *(int*)currentPtr = paramLen;
        currentPtr += 4;

        // Write String
        memcpy(currentPtr, param.c_str(), paramLen);
        currentPtr += paramLen;
    }

    return buffer;
}

void Command::ParseInput(const std::string& rawInput) {
    std::stringstream ss(rawInput);
    std::string token;
    bool isVerb = true;

    while (ss >> token) {
        if (isVerb) {
            this->CommandString = token;
            isVerb = false;
        }
        else {
            this->Parameters.push_back(token);
        }
    }
}

void Command::ParseInput(std::vector<std::string> p) {
    this->Parameters.clear(); 

    if (p.empty()) {
        this->CommandString.clear();
        return;
    }

    this->CommandString = p[0];


    for (size_t i = 1; i < p.size(); ++i) {
        this->Parameters.push_back(p[i]);
    }
}
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace telnet {

constexpr uint8_t IAC  = 255;
constexpr uint8_t DONT = 254;
constexpr uint8_t DO   = 253;
constexpr uint8_t WONT = 252;
constexpr uint8_t WILL = 251;
constexpr uint8_t SB   = 250;
constexpr uint8_t SE   = 240;
constexpr uint8_t GMCP = 201;

enum class Event {
    TextByte,
    Negotiation,
    SubnegStart,
    SubnegData,
    SubnegEnd,
};

struct Decoded {
    Event kind = Event::TextByte;
    uint8_t option = 0;
    std::vector<uint8_t> data;
    bool textByteEscaped = false;
};

class Decoder {
public:
    std::vector<Decoded> feed(const uint8_t* data, size_t len);

private:
    enum class State {
        Normal,
        GotIAC,
        GotIACCmd,
        GotSubnegOpt,
        InSubneg,
        InSubnegGotIAC,
    };

    State state = State::Normal;
    uint8_t pendingCmd = 0;
    uint8_t pendingOpt = 0;
    std::vector<uint8_t> subnegBuf;
};

std::string escapeText(const std::string& text);
std::string buildNegotiation(uint8_t cmd, uint8_t opt);
std::string buildGMCPSubneg(const std::string& module, const std::string& jsonPayload);

} // namespace telnet

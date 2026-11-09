#include "TelnetCodec.h"

namespace telnet {

std::string escapeText(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);

    for (size_t i = 0; i < text.size(); ++i) {
        uint8_t b = static_cast<uint8_t>(text[i]);
        if (b == IAC) {
            out.push_back(static_cast<char>(IAC));
            out.push_back(static_cast<char>(IAC));
        } else {
            out.push_back(static_cast<char>(b));
        }
    }
    return out;
}

std::string buildNegotiation(uint8_t cmd, uint8_t opt) {
    std::string out;
    out.reserve(3);
    out.push_back(static_cast<char>(IAC));
    out.push_back(static_cast<char>(cmd));
    out.push_back(static_cast<char>(opt));
    return out;
}

std::string buildGMCPSubneg(const std::string& module, const std::string& jsonPayload) {
    std::string out;
    out.reserve(jsonPayload.size() + module.size() + 8);
    out.push_back(static_cast<char>(IAC));
    out.push_back(static_cast<char>(SB));
    out.push_back(static_cast<char>(GMCP));
    out.push_back('"');
    out.append(module);
    out.push_back('"');
    out.push_back(' ');
    out.append(jsonPayload);
    out.push_back(static_cast<char>(IAC));
    out.push_back(static_cast<char>(SE));
    return out;
}

std::vector<Decoded> Decoder::feed(const uint8_t* data, size_t len) {
    std::vector<Decoded> events;
    if (!data || len == 0) return events;

    events.reserve(len / 4 + 1);

    for (size_t i = 0; i < len; ++i) {
        uint8_t b = data[i];

        switch (state) {
            case State::Normal: {
                if (b == IAC) {
                    state = State::GotIAC;
                } else {
                    Decoded d;
                    d.kind = Event::TextByte;
                    d.data.push_back(b);
                    events.push_back(std::move(d));
                }
                break;
            }

            case State::GotIAC: {
                if (b == IAC) {
                    Decoded d;
                    d.kind = Event::TextByte;
                    d.data.push_back(IAC);
                    d.textByteEscaped = true;
                    events.push_back(std::move(d));
                    state = State::Normal;
                } else if (b == WILL || b == WONT || b == DO || b == DONT) {
                    pendingCmd = b;
                    state = State::GotIACCmd;
                } else if (b == SB) {
                    subnegBuf.clear();
                    subnegBuf.reserve(256);
                    state = State::GotSubnegOpt;
                } else {
                    state = State::Normal;
                }
                break;
            }

            case State::GotIACCmd: {
                Decoded d;
                d.kind = Event::Negotiation;
                d.option = b;
                d.data.push_back(pendingCmd);
                events.push_back(std::move(d));
                state = State::Normal;
                break;
            }

            case State::GotSubnegOpt: {
                pendingOpt = b;
                Decoded d;
                d.kind = Event::SubnegStart;
                d.option = b;
                events.push_back(std::move(d));
                state = State::InSubneg;
                break;
            }

            case State::InSubneg: {
                if (b == IAC) {
                    state = State::InSubnegGotIAC;
                } else {
                    subnegBuf.push_back(b);
                }
                break;
            }

            case State::InSubnegGotIAC: {
                if (b == IAC) {
                    subnegBuf.push_back(IAC);
                    state = State::InSubneg;
                } else if (b == SE) {
                    Decoded d;
                    d.kind = Event::SubnegEnd;
                    d.option = pendingOpt;
                    d.data = subnegBuf;
                    events.push_back(std::move(d));
                    subnegBuf.clear();
                    state = State::Normal;
                } else {
                    subnegBuf.push_back(IAC);
                    subnegBuf.push_back(b);
                    state = State::InSubneg;
                }
                break;
            }
        }
    }

    return events;
}

} // namespace telnet

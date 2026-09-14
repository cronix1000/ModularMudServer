// Standalone tests for the TelnetCodec (parser + outbound escape).
// Build: included in CMakeLists as a non-default target.
// Run: ./test_telnet_codec

#include "TelnetCodec.h"

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

using telnet::Decoder;
using telnet::Decoded;
using telnet::Event;
using telnet::IAC;
using telnet::WILL;
using telnet::WONT;
using telnet::DO;
using telnet::DONT;
using telnet::SB;
using telnet::SE;
using telnet::GMCP;

using ByteVec = std::vector<unsigned char>;

static int errors = 0;
static int passed = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; ++errors; } \
    else { std::cout << "PASS: " << msg << std::endl; ++passed; } \
} while (0)

static std::string asString(const ByteVec& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

static ByteVec toBytes(const std::string& s) {
    return ByteVec(s.begin(), s.end());
}

static std::vector<Decoded> feedAll(Decoder& dec, const ByteVec& bytes) {
    return dec.feed(bytes.data(), bytes.size());
}

static std::vector<Decoded> feedAll(Decoder& dec, const std::string& s) {
    return feedAll(dec, toBytes(s));
}

static std::vector<Decoded> feedAll(Decoder& dec, const unsigned char* data, std::size_t len) {
    return dec.feed(reinterpret_cast<const uint8_t*>(data), len);
}

static size_t countOf(const std::vector<Decoded>& events, Event kind) {
    size_t n = 0;
    for (const auto& e : events) if (e.kind == kind) ++n;
    return n;
}

static const Decoded* firstOf(const std::vector<Decoded>& events, Event kind) {
    for (const auto& e : events) if (e.kind == kind) return &e;
    return nullptr;
}

int main() {
    // ----------------------------------------------------------------------
    // Case 1: plain text passes through as TextByte events
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        auto events = feedAll(dec, std::string("look\r\n"));
        CHECK(countOf(events, Event::TextByte) == 6, "plain text produces one TextByte per byte");
        CHECK(countOf(events, Event::Negotiation) == 0, "plain text yields no negotiation");
        CHECK(countOf(events, Event::SubnegStart) == 0, "plain text yields no subneg");
    }

    // ----------------------------------------------------------------------
    // Case 2: IAC IAC collapses to a single literal 0xFF TextByte, escaped
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> bytes = { IAC, IAC };
        auto events = feedAll(dec, bytes);
        CHECK(events.size() == 1, "IAC IAC yields exactly one event");
        CHECK(events[0].kind == Event::TextByte, "IAC IAC yields TextByte");
        CHECK(events[0].textByteEscaped, "TextByte flagged as escaped");
        CHECK(!events[0].data.empty() && events[0].data[0] == IAC, "Escaped byte is 0xFF");
    }

    // ----------------------------------------------------------------------
    // Case 3: IAC WILL GMCP produces a Negotiation event with option=GMCP
    //         and data[0]=WILL (the command).
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> bytes = { IAC, WILL, GMCP };
        auto events = feedAll(dec, bytes);
        CHECK(events.size() == 1, "IAC WILL GMCP yields exactly one event");
        CHECK(events[0].kind == Event::Negotiation, "kind is Negotiation");
        CHECK(events[0].option == GMCP, "option is GMCP (201)");
        CHECK(!events[0].data.empty() && events[0].data[0] == WILL, "command byte is WILL");
    }

    // ----------------------------------------------------------------------
    // Case 4: IAC DO ECHO produces a Negotiation event with option=ECHO(1).
    //         (We don't actually support ECHO; consumer decides to WONT/DONT.)
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> bytes = { IAC, DO, 1 };
        auto events = feedAll(dec, bytes);
        CHECK(events.size() == 1, "IAC DO ECHO yields one event");
        CHECK(events[0].kind == Event::Negotiation, "kind is Negotiation");
        CHECK(events[0].option == 1, "option is ECHO (1)");
        CHECK(!events[0].data.empty() && events[0].data[0] == DO, "command byte is DO");
    }

    // ----------------------------------------------------------------------
    // Case 5: IAC SB GMCP "<module>" <json> IAC SE produces SubnegStart,
    //         then a single SubnegEnd whose data is "<module> <json>".
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::string payload = "\"Char.Vitals\" {\"hp\":42}";
        std::vector<uint8_t> bytes;
        bytes.push_back(IAC); bytes.push_back(SB); bytes.push_back(GMCP);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        bytes.push_back(IAC); bytes.push_back(SE);

        auto events = feedAll(dec, bytes);
        CHECK(countOf(events, Event::SubnegStart) == 1, "GMCP packet has one SubnegStart");
        CHECK(countOf(events, Event::SubnegEnd) == 1, "GMCP packet has one SubnegEnd");

        const Decoded* start = firstOf(events, Event::SubnegStart);
        CHECK(start != nullptr && start->option == GMCP, "SubnegStart option is GMCP");

        const Decoded* end = firstOf(events, Event::SubnegEnd);
        CHECK(end != nullptr && end->option == GMCP, "SubnegEnd option is GMCP");
        CHECK(end != nullptr && asString(end->data) == payload, "SubnegEnd payload preserved verbatim");
    }

    // ----------------------------------------------------------------------
    // Case 6: Mixed stream — text, WILL GMCP, more text — all interleaved.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> bytes;
        std::string a = "look\r\n";
        std::string b = "north\n";
        bytes.insert(bytes.end(), a.begin(), a.end());
        bytes.push_back(IAC); bytes.push_back(WILL); bytes.push_back(GMCP);
        bytes.insert(bytes.end(), b.begin(), b.end());

        auto events = feedAll(dec, bytes);
        CHECK(countOf(events, Event::TextByte) == a.size() + b.size(), "interleaved text counts both lines");
        CHECK(countOf(events, Event::Negotiation) == 1, "interleaved stream has one Negotiation");

        const Decoded* neg = firstOf(events, Event::Negotiation);
        CHECK(neg != nullptr && neg->option == GMCP, "interleaved Negotiation is WILL GMCP");
    }

    // ----------------------------------------------------------------------
    // Case 7: IAC IAC inside a subnegotiation is preserved as a literal 0xFF
    //         in the subnegBuf payload.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> bytes = {
            IAC, SB, 99,                  // SB option 99 (arbitrary)
            'f', 'o', 'o',
            IAC, IAC,                     // literal 0xFF inside subneg
            'b', 'a', 'r',
            IAC, SE,
        };
        auto events = feedAll(dec, bytes);
        const Decoded* end = firstOf(events, Event::SubnegEnd);
        CHECK(end != nullptr, "IAC IAC inside subneg still produces SubnegEnd");
        CHECK(end != nullptr && end->data.size() == 7, "subneg payload preserves 7 bytes (foo + 0xFF + bar)");
        if (end) {
            std::vector<uint8_t> expected = {'f','o','o', IAC, 'b','a','r'};
            CHECK(end->data == expected, "subneg payload content matches 'foo'+0xFF+'bar'");
        }
    }

    // ----------------------------------------------------------------------
    // Case 8: Malformed — IAC SE without prior SB is dropped, state returns
    //         to Normal.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> bytes = { IAC, SE, 'h', 'i' };
        auto events = feedAll(dec, bytes);
        CHECK(countOf(events, Event::TextByte) == 2, "IAC SE outside subneg is dropped, 'hi' becomes 2 TextBytes");
        CHECK(countOf(events, Event::SubnegEnd) == 0, "IAC SE outside subneg does not produce SubnegEnd");
    }

    // ----------------------------------------------------------------------
    // Case 9: Buffer splits across the IAC boundary — chunk 1 ends with IAC,
    //         chunk 2 starts with WILL.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> chunk1 = { 'h', 'i', IAC };
        std::vector<uint8_t> chunk2 = { WILL, GMCP, 'x' };

        auto e1 = feedAll(dec, chunk1);
        auto e2 = feedAll(dec, chunk2);

        CHECK(countOf(e1, Event::TextByte) == 2, "split chunk 1 yields 'hi' as TextBytes");
        CHECK(countOf(e1, Event::Negotiation) == 0, "split chunk 1 has no Negotiation yet");

        CHECK(countOf(e2, Event::TextByte) == 1, "split chunk 2 yields 'x' as TextByte");
        CHECK(countOf(e2, Event::Negotiation) == 1, "split chunk 2 completes the Negotiation");
        const Decoded* neg = firstOf(e2, Event::Negotiation);
        CHECK(neg != nullptr && neg->option == GMCP, "split Negotiation completes with option=GMCP");
    }

    // ----------------------------------------------------------------------
    // Case 10: Buffer splits across an option byte — chunk 1 ends with DO,
    //          chunk 2 starts with the option code.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> chunk1 = { IAC, DO };
        std::vector<uint8_t> chunk2 = { 31 /* NAWS */, 'x' };

        auto e1 = feedAll(dec, chunk1);
        auto e2 = feedAll(dec, chunk2);

        CHECK(countOf(e1, Event::Negotiation) == 0, "split chunk 1 has no complete Negotiation");
        CHECK(countOf(e2, Event::Negotiation) == 1, "split chunk 2 completes Negotiation");
        const Decoded* neg = firstOf(e2, Event::Negotiation);
        CHECK(neg != nullptr && neg->option == 31, "split Negotiation option is NAWS (31)");
        CHECK(countOf(e2, Event::TextByte) == 1, "split chunk 2 yields 'x' as TextByte");
    }

    // ----------------------------------------------------------------------
    // Case 11: Buffer splits inside a subnegotiation — the SubnegEnd does
    //          not appear until the chunk that contains IAC SE.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        std::vector<uint8_t> chunk1 = { IAC, SB, GMCP, '"', 'M', '"', ' ' };
        std::vector<uint8_t> chunk2 = { '{', '"', 'h', 'p', '"', ':', '1', '}' };
        std::vector<uint8_t> chunk3 = { IAC, SE, 'X' };

        auto e1 = feedAll(dec, chunk1);
        auto e2 = feedAll(dec, chunk2);
        auto e3 = feedAll(dec, chunk3);

        CHECK(countOf(e1, Event::SubnegStart) == 1, "split chunk 1 emits SubnegStart");
        CHECK(countOf(e1, Event::SubnegEnd) == 0, "split chunk 1 has no SubnegEnd yet");
        CHECK(countOf(e2, Event::SubnegEnd) == 0, "split chunk 2 has no SubnegEnd yet");
        CHECK(countOf(e3, Event::SubnegEnd) == 1, "split chunk 3 emits SubnegEnd");

        const Decoded* end = firstOf(e3, Event::SubnegEnd);
        CHECK(end != nullptr && asString(end->data) == "\"M\" {\"hp\":1}",
              "split subneg payload is reassembled exactly");
        CHECK(countOf(e3, Event::TextByte) == 1, "post-subneg text 'X' is one TextByte");
    }

    // ----------------------------------------------------------------------
    // Case 12: Empty input is a no-op.
    // ----------------------------------------------------------------------
    {
        Decoder dec;
        auto e1 = feedAll(dec, std::vector<uint8_t>{});
        auto e2 = feedAll(dec, static_cast<const uint8_t*>(nullptr), 0);
        CHECK(e1.empty(), "empty vector yields no events");
        CHECK(e2.empty(), "nullptr+0 yields no events");
    }

    // ----------------------------------------------------------------------
    // Outbound: escapeText doubles every 0xFF
    // ----------------------------------------------------------------------
    {
        std::string out = telnet::escapeText("hello\xFFworld");
        CHECK(out.size() == 12, "escapeText: input 11 bytes becomes 12 (one 0xFF doubled)");
        std::string expected = "hello\xFF\xFFworld";
        CHECK(out == expected, "escapeText: content is hello + IAC IAC + world");
    }

    // ----------------------------------------------------------------------
    // Outbound: escapeText is a no-op on text without 0xFF
    // ----------------------------------------------------------------------
    {
        std::string in = "look north";
        std::string out = telnet::escapeText(in);
        CHECK(out == in, "escapeText: text without 0xFF is unchanged");
    }

    // ----------------------------------------------------------------------
    // Outbound: buildNegotiation produces the canonical 3-byte sequence.
    // ----------------------------------------------------------------------
    {
        std::string s = telnet::buildNegotiation(WILL, GMCP);
        CHECK(s.size() == 3, "buildNegotiation produces 3 bytes");
        CHECK(static_cast<uint8_t>(s[0]) == IAC, "buildNegotiation[0] is IAC");
        CHECK(static_cast<uint8_t>(s[1]) == WILL, "buildNegotiation[1] is WILL");
        CHECK(static_cast<uint8_t>(s[2]) == GMCP, "buildNegotiation[2] is GMCP");
    }

    {
        std::string s = telnet::buildNegotiation(DO, GMCP);
        CHECK(s.size() == 3, "buildNegotiation(DO,GMCP) produces 3 bytes");
        CHECK(static_cast<uint8_t>(s[1]) == DO, "buildNegotiation(DO,GMCP)[1] is DO");
    }

    // ----------------------------------------------------------------------
    // Outbound: buildGMCPSubneg produces IAC SB GMCP "<module>" <json> IAC SE.
    // ----------------------------------------------------------------------
    {
        std::string s = telnet::buildGMCPSubneg("Char.Vitals", "{\"hp\":42}");
        std::string expected;
        expected.push_back(static_cast<char>(IAC));
        expected.push_back(static_cast<char>(SB));
        expected.push_back(static_cast<char>(GMCP));
        expected.append("\"Char.Vitals\" {\"hp\":42}");
        expected.push_back(static_cast<char>(IAC));
        expected.push_back(static_cast<char>(SE));
        CHECK(s == expected, "buildGMCPSubneg emits canonical IAC SB GMCP frame");
    }

    // ----------------------------------------------------------------------
    // Round-trip: a frame built by buildGMCPSubneg and fed through the
    // Decoder yields SubnegStart + SubnegEnd with the original module+json
    // payload intact.
    // ----------------------------------------------------------------------
    {
        std::string frame = telnet::buildGMCPSubneg("Room.Info", "{\"num\":3}");
        Decoder dec;
        auto events = feedAll(dec, frame);

        CHECK(countOf(events, Event::SubnegStart) == 1, "round-trip: one SubnegStart");
        CHECK(countOf(events, Event::SubnegEnd) == 1, "round-trip: one SubnegEnd");
        const Decoded* end = firstOf(events, Event::SubnegEnd);
        CHECK(end != nullptr && end->option == GMCP, "round-trip: SubnegEnd option is GMCP");
        CHECK(end != nullptr && asString(end->data) == "\"Room.Info\" {\"num\":3}",
              "round-trip: payload preserved with quotes and space");
    }

    std::cout << "\n--- " << passed << " passed, " << errors << " failed ---\n";
    return errors == 0 ? 0 : 1;
}

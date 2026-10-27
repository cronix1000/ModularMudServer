#pragma once
#include <string>
#include <memory>

class ClientConnection;
namespace ClientInputKind {
    enum Kind {
        Line,
        Disconnected,
        Connected
    };
}

struct ClientInput {
    int clientID = -1;
    std::string rawText; // This holds "password123", "kill goblin", or "yes"
    ClientInputKind::Kind kind = ClientInputKind::Kind::Line;
    std::shared_ptr<ClientConnection> connection; // only set for Kind Connection
};

struct OutboundPacket {
    std::string data;
    bool escapeTelnet;
};
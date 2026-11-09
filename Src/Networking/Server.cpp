#include "Server.h"
#include "ClientConnection.h"
#include "Registry.h"
#include <chrono>
#include "PlayerComponents.h"
#include "ClientInput.h"
#include "MainMenuState.h"
#include "GameContext.h"
#include <unordered_map>
#include "ThreadSafeQueue.h"
#include "TelnetCodec.h"
#include <stop_token>

Server::Server(GameContext& context, GameEngine* engine, ThreadSafeQueue<ClientInput>& queue) : gameContext(context), engine(engine), inputQueue(queue) {
}

Server::~Server() {

}

void Server::HandleReceive(int clientID, std::string& buffer) {
    size_t pos = 0;
    while ((pos = buffer.find('\n')) != std::string::npos) {
        // 1. Extract raw string
        std::string line = buffer.substr(0, pos);
        // ... trim \r ...

        // 2. Push to Queue (No parsing, no logic)
        ClientInput input;
        input.clientID = clientID;
        input.rawText = line; // Just the string!

        this->inputQueue.Push(input); // Thread-safe push

        buffer.erase(0, pos + 1);
    }
}

void Server::Run(std::stop_token stop_token) {
    printf("Running server....");
    while (!stop_token.stop_requested()) {
        fd_set read_fd;
        fd_set write_fd; // Use a single write set
        FD_ZERO(&read_fd);
        FD_ZERO(&write_fd);

        FD_SET(ListenSocket, &read_fd);
        SocketType max_fd = ListenSocket;

        // Populate the read_fd and write_fd sets
        for (const auto& pair : activeClients) {
            std::shared_ptr<ClientConnection> client = pair.second;
            SocketType client_socket = client->tcpSocket;

            // ALWAYS check for reading
            FD_SET(client_socket, &read_fd);

            // ONLY check for writing if the client has queued data
            if (!client->outbound.IsEmpty()) {
                FD_SET(client_socket, &write_fd);
            }

            if (client_socket > max_fd) {
                max_fd = client_socket;
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000; // 1ms timeout (1000 times per second tick)

        // **CALL SELECT ONLY ONCE**
        int sockets_ready = select(
            static_cast<int>(max_fd) + 1,
            &read_fd,   // Check for reading
            &write_fd,  // Check for writing
            NULL,       // Check for errors
            &timeout
        );

        // SOCKET ERROR 
        if (sockets_ready == SOCKET_ERROR_VAL) {
            printf("select failed with error: %d\n", GetSocketError());
            break; // Exit the loop
        }

        // SOCKET Timeout
        //if (sockets_ready == 0) {
        //    // Run non-I/O game logic here (GameWorld::Tick())
        //    continue;
        //}

        if (FD_ISSET(ListenSocket, &read_fd)) {
            AcceptClient();
            sockets_ready--; // because listen socket is used here
        }

        //std::vector<ClientConnection> tempClients = activeClients;
        // check if clients sent data and recieve it
        for (auto it = activeClients.begin(); it != activeClients.end() && sockets_ready > 0;) {
            std::shared_ptr<ClientConnection> client = it->second;
            bool disconnected = false;
            SocketType socket = client->tcpSocket;

            if (client->needsCleanup) {
                disconnected = true;
            }

            // 1. Process READ activity
            if (!disconnected && FD_ISSET(socket, &read_fd)) {
                int bytes_processed = client->RecieveData();
                if (bytes_processed <= 0) {
                    disconnected = true;
                }
                else {
                    // Process the input that was received
					//this->inputQueue.Push({client->clientID, client->recvBuffer});
                    for(std::string line : client->DrainCompleteLines()) {
                        this->inputQueue.Push({ client->clientID , std::move(line), ClientInputKind::Kind::Line });
					}
                }
            }

            // 2. Process WRITE activity
            if (!disconnected && FD_ISSET(socket, &write_fd)) {
                int bytes_sent = client->SendData();
                if (bytes_sent < 0) {
                    disconnected = true;
                    inputQueue.Push({ client->clientID, "", ClientInputKind::Kind::Disconnected });
                    
                }
            }

            // 3. Handle Disconnection
            if (disconnected) {
                printf("Client Disconnected\n");
                inputQueue.Push({ client->clientID, "", ClientInputKind::Kind::Disconnected });
                it = activeClients.erase(it);
            }
            else {
                ++it;
            }

        }
    }

    for (auto& [id, client] : activeClients) {
        client->SendData();
    }
}

bool Server::Start(const char* DEFAULT_PORT) {
    int iResult;

    ListenSocket = INVALID_SOCKET_VAL;
    SocketType ClientSocket = INVALID_SOCKET_VAL;
    struct addrinfo* result = NULL;
    struct addrinfo hints;

    // Initialize Sockets (platform-specific)
#ifdef PLATFORM_WINDOWS
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return false;
    }
#endif

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
#ifdef PLATFORM_WINDOWS
        WSACleanup();
#endif
        return false;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET_VAL) {
        printf("socket failed with error: %d\n", GetSocketError());
        freeaddrinfo(result);
        SocketCleanup();
        return false;
    }

    // Allow socket reuse (helps with "address already in use" errors)
    int opt = 1;
    if (setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, 
#ifdef PLATFORM_WINDOWS
        (const char*)
#else
        (const void*)
#endif
        &opt, sizeof(opt)) < 0) {
        printf("setsockopt failed with error: %d\n", GetSocketError());
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen));
    if (iResult == SOCKET_ERROR_VAL) {
        printf("bind failed with error: %d\n", GetSocketError());
        freeaddrinfo(result);
        CloseSocket(ListenSocket);
        SocketCleanup();
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR_VAL) {
        printf("listen failed with error: %d\n", GetSocketError());
        CloseSocket(ListenSocket);
        SocketCleanup();
        return false;
    }

    printf("Server Started");
    return true;
}

bool Server::Stop() {
    if (ListenSocket != INVALID_SOCKET_VAL) {
        CloseSocket(ListenSocket);
        ListenSocket = INVALID_SOCKET_VAL;
    }
    return true;

}

bool Server::AcceptClient() {
    SocketType newSocket = accept(ListenSocket, NULL, NULL);
    if (newSocket != INVALID_SOCKET_VAL) {
		auto newClient = std::make_shared<ClientConnection>(newSocket);
        // Set a unique client ID (you can use the socket number or a counter)
        newClient->clientID = static_cast<int>(newSocket);

        newClient->SetEngine(engine);
        inputQueue.Push({ newClient->clientID , "", ClientInputKind::Kind::Connected, newClient });

        // Offer GMCP. Real telnet clients (Mudlet etc.) reply IAC DO GMCP
        // and our Negotiation handler flips ClientComponent.hasGMCP = true.
        // Browser clients via the gateway ignore these bytes.
        // Use SendPacket (not QueueMessage) to bypass the outbound escape pass,
        // which would otherwise double every IAC byte and corrupt the frame.
        newClient->SendPacket({telnet::buildNegotiation(telnet::WILL, telnet::GMCP), false});
        newClient->SendPacket({telnet::buildNegotiation(telnet::DO,   telnet::GMCP), false});

        activeClients[newClient->clientID] = newClient;

        printf("New client connected with ID: %d\n", newClient->clientID);
        return true;
    }
    else {
        return false;
    }
}

#include "Server.h"
#include "Registry.h"
#include <chrono>
#include "ClientComponent.h"
#include "ClientInput.h"
#include "MainMenuState.h"
#include "GameContext.h"
#include "ThreadSafeQueue.h"
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
void Server::Run() {
    printf("Running server....");
    while (true) {
        fd_set read_fd;
        fd_set write_fd; // Use a single write set
        FD_ZERO(&read_fd);
        FD_ZERO(&write_fd);

        FD_SET(ListenSocket, &read_fd);
        SOCKET max_fd = ListenSocket;

        // Populate the read_fd and write_fd sets
        for (ClientConnection* client : activeClients) {
            SOCKET client_socket = client->tcpSocket;

            // ALWAYS check for reading
            FD_SET(client_socket, &read_fd);

            // ONLY check for writing if the client has queued data
            if (!client->OutboundMessages.empty()) {
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
            max_fd + 1,
            &read_fd,   // Check for reading
            &write_fd,  // Check for writing
            NULL,       // Check for errors
            &timeout
        );

        // SOCKET ERROR 
        if (sockets_ready == SOCKET_ERROR) {
            printf("select failed with error: %d\n", WSAGetLastError());
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
            ClientConnection* client = *it;
            bool disconnected = false;
            SOCKET socket = client->tcpSocket;

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
                    HandleReceive(client->clientID, client->recvBuffer);
                    client->ProcessInput();  // Then process it
                }
            }

            // 2. Process WRITE activity
            if (!disconnected && FD_ISSET(socket, &write_fd)) {
                int bytes_sent = client->SendData();
                if (bytes_sent < 0) {
                    disconnected = true;
                }
            }

            // 3. Handle Disconnection
            if (disconnected) {
                printf("Client Disconnected\n");

                if (client->playerEntityID != -1) {
                    gameContext.registry->RemoveComponent<ClientComponent>(client->playerEntityID);
                }

                delete client;
                it = activeClients.erase(it);
            }
            else {
                ++it;
            }

        }
    }
}

bool Server::Start(const char* DEFAULT_PORT) {
    WSADATA wsaData;
    int iResult;

    ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Server Started");
    return 0;
}

bool Server::Stop() {
    return 1;
}

bool Server::AcceptClient() {
    SOCKET newSocket = accept(ListenSocket, NULL, NULL);
    if (newSocket != INVALID_SOCKET) {
        ClientConnection* newClient = new ClientConnection(newSocket);

        // Set a unique client ID (you can use the socket number or a counter)
        newClient->clientID = static_cast<int>(newSocket); 

        newClient->SetEngine(engine);
        newClient->PushState(new MainMenuState());
        activeClients.push_back(newClient);

        printf("New client connected with ID: %d\n", newClient->clientID);
        return true;
    }
    else {
        return false;
    }
}

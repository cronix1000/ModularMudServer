#include "ClientConnection.h"

#include "GameEngine.h"  
#include "GameState.h"   
#include <cstring>       
#include <cstdio>   

int ClientConnection::RecieveData() {
    char recvbuf[DEFAULT_BUFLEN];
    // Clear buffer not strictly necessary if we use the return value correctly, 
    // but good for safety.
    memset(recvbuf, 0, DEFAULT_BUFLEN);

    int bytesReceived = recv(this->tcpSocket, recvbuf, DEFAULT_BUFLEN - 1, 0);

    if (bytesReceived > 0) {
        // 1. Append raw bytes to our persistent buffer
        inputBuffer.append(recvbuf, bytesReceived);

        // 2. Process ALL complete messages in the buffer
        size_t pos = 0;

        return bytesReceived;
    }
    else {
        return 0; // Disconnected or error
    }
}


void ClientConnection::ProcessInput() {
    size_t pos = 0;

    // Find position of the next newline character
    while ((pos = inputBuffer.find('\n')) != std::string::npos) {

        // 1. Extract the command line (up to the \n)
        std::string line = inputBuffer.substr(0, pos);

        // 2. Remove the processed part from the buffer (including the \n at pos)
        inputBuffer.erase(0, pos + 1);

        // 3. Handle Telnet \r (Carriage Return) if present at the end
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 4. Skip processing if the line is empty (user just hit enter)
        if (line.empty()) continue;

        // 5. Tokenize (Split string by spaces into vector)
        std::vector<std::string> parameters;
        std::stringstream ss(line);
        std::string token;
        while (ss >> token) {
            parameters.push_back(token);
        }

        // 6. Safety Check: Ensure we actually have words before processing
        if (parameters.empty()) continue;

        // 8. Pass to the active Game State (Menu or Playing)
        if (!stateStack.empty()) {
            stateStack.top()->HandleInput(this, parameters);
        }
    }
}
int ClientConnection::SendData() {

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN] = { 0 };
    int recvbuflen = DEFAULT_BUFLEN;
    int iResult = 0;
    std::string hugePacket = "";

    while (!OutboundMessages.empty()) {
        hugePacket += OutboundMessages.front();
        OutboundMessages.pop();
    }

    // 3. Send ONLY ONCE
    if (!hugePacket.empty()) {
        iSendResult = send(this->tcpSocket, hugePacket.c_str(), static_cast<int>(hugePacket.size()), 0);

        if (iSendResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(this->tcpSocket);
            WSACleanup();
            return iResult;
        }
        printf("Bytes sent: %d\n", iSendResult);
    }
}

void ClientConnection::QueueMessage(const std::string& msg) {
    OutboundMessages.push(msg);
}
void ClientConnection::DisconnectGracefully() {
    // 1. Send the TCP shutdown signal (SD_SEND)
    shutdown(this->tcpSocket, SD_SEND);

    this->needsCleanup = true;
}

void ClientConnection::PopState() {
    if (stateStack.empty()) return;

    // 1. Clean up the current state
    GameState* oldState = stateStack.top();
    delete oldState; // This triggers the destructor of the popped state

    // 2. Remove it
    stateStack.pop();

    // 3. WAKE UP the state that is now on top
    if (!stateStack.empty()) {
        stateStack.top()->OnResume(this);
    }

}

void ClientConnection::PushState(GameState* state) {
    stateStack.push(state);

    state->OnEnter(this);
}
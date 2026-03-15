#pragma once

// Platform detection
#ifdef _WIN32
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__unix__)
    #define PLATFORM_LINUX
#else
    #error "Unsupported platform"
#endif

// Windows-specific includes and definitions
#ifdef PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    // Link with Ws2_32.lib
    #pragma comment(lib, "Ws2_32.lib")
    
    // Socket types
    typedef SOCKET SocketType;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    
    // Functions
    #define CloseSocket closesocket
    #define GetSocketError() WSAGetLastError()
    #define SocketCleanup() WSACleanup()
    #define InitSockets() \
        do { \
            WSADATA wsaData; \
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false; \
        } while(0)
    
    // Timing
    #define GetTickCountMs() GetTickCount64()
    #define SleepMs(ms) Sleep(ms)
    
// Linux-specific includes and definitions
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <netdb.h>
    #include <string.h>
    #include <chrono>
    #include <thread>
    
    // Socket types (POSIX)
    typedef int SocketType;
    #define INVALID_SOCKET_VAL (-1)
    #define SOCKET_ERROR_VAL (-1)
    
    // Functions
    #define CloseSocket close
    #define GetSocketError() errno
    #define SocketCleanup() do {} while(0)
    #define InitSockets() do {} while(0)
    
    // For compatibility with Windows socket functions
    #define SD_SEND SHUT_WR
    #define SD_RECEIVE SHUT_RD
    #define SD_BOTH SHUT_RDWR
    
    // Timing (Linux)
    inline unsigned long long GetTickCountMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    
    inline void SleepMs(unsigned int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    
    // ZeroMemory equivalent
    #define ZeroMemory(dest, len) memset((dest), 0, (len))
#endif

// Common socket-related defines
#define DEFAULT_BUFLEN 1024

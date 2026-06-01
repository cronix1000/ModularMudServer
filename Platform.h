#pragma once

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define PLATFORM_LINUX
#else
    #error "Unsupported platform"
#endif

#ifdef PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    // winsock2.h must come BEFORE windows.h
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <string.h>

    #pragma comment(lib, "Ws2_32.lib")

    // Unified socket type
    typedef SOCKET socket_t;
    typedef SOCKET SocketType;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL  SOCKET_ERROR

    // Function wrappers (both naming styles)
    #define close_socket(s)        closesocket(s)
    #define CloseSocket(s)         closesocket(s)
    #define init_sockets()         do { WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData); } while(0)
    #define InitSockets()          do { WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData); } while(0)
    #define cleanup_sockets()      WSACleanup()
    #define SocketCleanup()        WSACleanup()
    #define get_socket_error()     WSAGetLastError()
    #define GetSocketError()       WSAGetLastError()

    #define get_tick_ms()          GetTickCount64()
    #define sleep_ms(ms)           Sleep(ms)
    #define GetTickCountMs()       GetTickCount64()
    #define SleepMs(ms)            Sleep(ms)

    // SHUT_* are aliases for Windows SD_* constants
    // (SD_SEND, SD_RECEIVE, SD_BOTH are already defined in winsock2.h)
    #ifndef SHUT_RD
        #define SHUT_RD    SD_RECEIVE
    #endif
    #ifndef SHUT_WR
        #define SHUT_WR    SD_SEND
    #endif
    #ifndef SHUT_RDWR
        #define SHUT_RDWR  SD_BOTH
    #endif
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
    #include <cstring>
    #include <chrono>
    #include <thread>

    // Linux: socket is int
    typedef int socket_t;
    typedef int SocketType;
    #define INVALID_SOCKET_VAL  (-1)
    #define SOCKET_ERROR_VAL    (-1)
    #define INVALID_SOCKET      (-1)
    #define SOCKET_ERROR        (-1)

    // Function wrappers
    #define close_socket(s)        ::close(s)
    #define CloseSocket(s)         ::close(s)
    #define init_sockets()         do {} while(0)
    #define InitSockets()          do {} while(0)
    #define cleanup_sockets()      do {} while(0)
    #define SocketCleanup()        do {} while(0)
    #define get_socket_error()     errno
    #define GetSocketError()       errno

    inline unsigned long long get_tick_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    inline unsigned long long GetTickCountMs() { return get_tick_ms(); }
    inline void sleep_ms(unsigned int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
    inline void SleepMs(unsigned int ms) { sleep_ms(ms); }

    // SD_* aliases for Windows code compatibility
    #ifndef SD_SEND
        #define SD_SEND    SHUT_WR
    #endif
    #ifndef SD_RECEIVE
        #define SD_RECEIVE SHUT_RD
    #endif
    #ifndef SD_BOTH
        #define SD_BOTH    SHUT_RDWR
    #endif

    // ZeroMemory alias
    #ifndef ZeroMemory
        #define ZeroMemory(dest, len) memset((dest), 0, (len))
    #endif
#endif

#define DEFAULT_BUFLEN 1024

// SOCKET typedef for old code compatibility
#ifdef PLATFORM_WINDOWS
    // SOCKET is already defined by winsock2.h
#else
    typedef socket_t SOCKET;
#endif

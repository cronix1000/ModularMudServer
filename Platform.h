#pragma once

// ============================================================
// Cross-platform abstraction layer
// Works on Windows (MSVC, MinGW) and Linux (GCC, Clang)
// ============================================================

// Platform detection
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define PLATFORM_LINUX
#else
    #error "Unsupported platform"
#endif

// ============================================================
// WINDOWS
// ============================================================
#ifdef PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef _WINSOCKAPI_
        #define _WINSOCKAPI_   // Prevent winsock1 redefinition
    #endif

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <string.h>

    #pragma comment(lib, "Ws2_32.lib")

    // Unified socket type (just an alias for Windows SOCKET)
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL  SOCKET_ERROR

    // Function wrappers
    #define close_socket(s)        closesocket(s)
    #define init_sockets()         do { WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData); } while(0)
    #define cleanup_sockets()      WSACleanup()
    #define get_socket_error()     WSAGetLastError()

    // Timing
    #define get_tick_ms()          GetTickCount64()
    #define sleep_ms(ms)           Sleep(ms)

    // Shutdown constants
    #define SHUT_RD_WR             SD_BOTH
    #define SHUT_WR_ONLY           SD_SEND
    #define SHUT_RD_ONLY           SD_RECEIVE

// ============================================================
// LINUX / UNIX / MAC
// ============================================================
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

    // Unified socket type
    typedef int socket_t;
    #define INVALID_SOCKET_VAL  (-1)
    #define SOCKET_ERROR_VAL    (-1)
    #define INVALID_SOCKET      (-1)
    #define SOCKET_ERROR        (-1)

    // Function wrappers
    #define close_socket(s)        ::close(s)
    #define init_sockets()         do {} while(0)
    #define cleanup_sockets()      do {} while(0)
    #define get_socket_error()     errno

    // Timing
    inline unsigned long long get_tick_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    inline void sleep_ms(unsigned int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    // Shutdown constants (already defined in sys/socket.h, but alias for compat)
    // SHUT_RD, SHUT_WR, SHUT_RDWR are already in sys/socket.h
#endif

// ============================================================
// COMMON (both platforms)
// ============================================================
#define DEFAULT_BUFLEN 1024

// Compatibility macros for the existing codebase
// These let the existing code (which uses Windows names) work on both platforms
#ifdef PLATFORM_WINDOWS
    // Already natively available
    #define GetTickCountMs()  GetTickCount64()
    #define SleepMs(ms)       Sleep(ms)
#else
    // Linux/macOS implementations
    inline unsigned long long GetTickCountMs() {
        return get_tick_ms();
    }
    inline void SleepMs(unsigned int ms) {
        sleep_ms(ms);
    }
    // SOCKET type alias for compatibility with existing Windows-style code
    typedef socket_t SOCKET;
#endif

# Cross-Platform Build Changes

This document summarizes the changes made to support Linux builds while maintaining Windows compatibility.

## New Files Created

### 1. `Platform.h`
Platform abstraction header that provides:
- **Socket abstraction**: `SocketType`, `INVALID_SOCKET_VAL`, `SOCKET_ERROR_VAL`
- **Network functions**: `CloseSocket`, `GetSocketError`, `SocketCleanup`, `InitSockets`
- **Timing functions**: `GetTickCountMs()`, `SleepMs()`
- **Automatic platform detection** via preprocessor defines

### 2. `CMakeLists.txt`
Cross-platform CMake build configuration:
- Supports both Windows (Visual Studio) and Linux (GCC/Clang)
- Finds all required dependencies (SQLite3, nlohmann-json, Lua, sol2)
- Platform-specific library linking (ws2_32 on Windows, pthread on Linux)
- Copies data files and scripts to build directory

### 3. `build.sh`
Linux build script with:
- Dependency checking
- Automatic CMake configuration
- Parallel compilation
- Helpful error messages for missing dependencies

### 4. `BUILD_LINUX.md`
Documentation for building on Linux

## Modified Files

### `Server.h`
- Removed Windows-specific includes (`windows.h`, `winsock2.h`, `ws2tcpip.h`)
- Now includes `Platform.h` instead

### `Server.cpp`
- Updated to use platform-agnostic types (`SocketType` instead of `SOCKET`)
- Updated all socket calls to use abstraction macros
- Added platform-specific socket initialization

### `ClientConnection.h`
- Replaced `#include <WinSock2.h>` with `#include "Platform.h"`
- Changed `SOCKET tcpSocket` to `SocketType tcpSocket`
- Updated constructor and destructor to use platform macros

### `ClientConnection.cpp`
- Updated socket operations to use platform abstraction
- Added missing `#include <sstream>`

### `Main.cpp`
- Removed Windows-specific pragma comment for library linking
- Removed `__cdecl` calling convention specifier (Windows-specific)
- Updated timing calls to use `GetTickCountMs()` and `SleepMs()`

### `vcpkg.json`
- Added name and version fields for better package management

## How It Works

The `Platform.h` header uses preprocessor directives to detect the platform:

```cpp
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    // Windows includes and definitions
#else
    #define PLATFORM_LINUX
    // Linux includes and definitions
#endif
```

All platform-specific code is isolated in this single header, making the rest of the codebase clean and portable.

## Building

### Windows (existing Visual Studio project)
- Open `ModularMudServer.sln` in Visual Studio
- Build as usual

### Windows (CMake - NEW)
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Linux (NEW)
```bash
chmod +x build.sh
./build.sh
```

Or manually:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

## Dependencies

The project requires:
- C++17 compiler
- CMake 3.14+
- nlohmann-json
- SQLite3
- Lua 5.3+
- sol2 (header-only)

These can be installed via:
- **vcpkg** (Windows/Linux): `vcpkg install nlohmann-json sqlite3 sol2 lua`
- **apt** (Ubuntu/Debian): `sudo apt-get install nlohmann-json3-dev libsqlite3-dev liblua5.3-dev sol2`
- **dnf** (Fedora): `sudo dnf install nlohmann-json-devel sqlite-devel lua-devel sol2`
- **pacman** (Arch): `sudo pacman -S nlohmann-json sqlite lua sol2`

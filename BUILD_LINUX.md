# Building on Linux

This project now supports both Windows (Visual Studio) and Linux builds.

## Quick Start

```bash
# Clone the repository
git clone <repo-url>
cd ModularMudServer

# Build
chmod +x build.sh
./build.sh
```

## Manual Build

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

## Dependencies

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake \
    nlohmann-json3-dev libsqlite3-dev liblua5.3-dev sol2
```

### Fedora
```bash
sudo dnf install cmake nlohmann-json-devel sqlite-devel lua-devel sol2
```

### Arch Linux
```bash
sudo pacman -S cmake nlohmann-json sqlite lua sol2
```

### Using vcpkg
```bash
vcpkg install nlohmann-json sqlite3 sol2 lua
```

## Platform Abstraction

The codebase uses `Platform.h` for cross-platform compatibility:
- **Networking**: Abstracts Windows Winsock2 vs POSIX sockets
- **Timing**: Abstracts Windows `GetTickCount64`/`Sleep` vs Linux equivalents
- **Types**: `SocketType`, `INVALID_SOCKET_VAL`, `SOCKET_ERROR_VAL`

## Running

```bash
cd build/bin
./ModularMudServer
```

The server will listen on port 27015 by default.

## CMake Options

- `-DCMAKE_BUILD_TYPE=Release` - Release build (optimized)
- `-DCMAKE_BUILD_TYPE=Debug` - Debug build
- `-DCMAKE_PREFIX_PATH=/path/to/vcpkg/installed/x64-linux` - Use vcpkg packages

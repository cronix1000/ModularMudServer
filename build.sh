#!/bin/bash

# Build script for ModularMudServer on Linux

set -e

echo "=== ModularMudServer Linux Build Script ==="
echo

# Check for dependencies
check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        echo "ERROR: $1 is not installed"
        return 1
    fi
    echo "✓ Found $1"
}

echo "Checking dependencies..."
check_dependency cmake
check_dependency g++
echo

# Create build directory
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure
echo "Configuring with CMake..."
if ! cmake .. -DCMAKE_BUILD_TYPE=Release; then
    echo
    echo "ERROR: CMake configuration failed!"
    echo
    echo "You may need to install dependencies:"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential cmake nlohmann-json3-dev libsqlite3-dev liblua5.3-dev sol2"
    echo "  Fedora:        sudo dnf install cmake nlohmann-json-devel sqlite-devel lua-devel sol2"
    echo "  Arch:          sudo pacman -S cmake nlohmann-json sqlite lua sol2"
    echo
    echo "Or install via vcpkg:"
    echo "  vcpkg install nlohmann-json sqlite3 sol2 lua"
    exit 1
fi

# Build
echo
echo "Building..."
if cmake --build . --parallel "$(nproc)"; then
    echo
    echo "=== BUILD SUCCESSFUL ==="
    echo "Executable: $PWD/bin/ModularMudServer"
    echo
    echo "To run the server:"
    echo "  cd bin && ./ModularMudServer"
else
    echo
    echo "=== BUILD FAILED ==="
    exit 1
fi

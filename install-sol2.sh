#!/bin/bash
# install-sol2.sh
# Manual install of sol2 header-only library
# sol2 is not in apt repos for Debian 12, so we install from source

set -e

SOL2_VERSION="v3.3.1"
INSTALL_PREFIX="/usr/local"

echo "🔧 Installing sol2 (header-only C++ Lua bindings)..."
echo "Version: $SOL2_VERSION"
echo ""

# Try to use sudo if available
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if command -v sudo &> /dev/null; then
    SUDO="sudo"
  else
    echo "⚠️  Not root and no sudo. Will install to ~/.local instead."
    INSTALL_PREFIX="$HOME/.local"
  fi
fi

# Create temp dir
TMPDIR=$(mktemp -d)
cd "$TMPDIR"

echo "📥 Cloning sol2 from GitHub..."
git clone --depth 1 --branch "$SOL2_VERSION" https://github.com/ThePhD/sol2.git

cd sol2

# sol2 is header-only, just copy headers
echo "📋 Copying headers to $INSTALL_PREFIX/include..."
$SUDO mkdir -p "$INSTALL_PREFIX/include"
$SUDO cp -r include/sol "$INSTALL_PREFIX/include/"

# Verify
if [ -f "$INSTALL_PREFIX/include/sol/sol.hpp" ]; then
  echo ""
  echo "✅ sol2 installed successfully!"
  echo "   Header: $INSTALL_PREFIX/include/sol/sol.hpp"
  echo ""
  echo "You can now build your project:"
  echo "  cd ~/ModularMudServer"
  echo "  ./build.sh"
fi

# Cleanup
cd /
rm -rf "$TMPDIR"

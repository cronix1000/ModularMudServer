#!/bin/bash
# setup-mud-repo.sh
# Clone the MUD repo, build it, and set up the deployment

set -e

REPO_URL="git@github.com:cronix10000/ModularMudServer.git"  # Update with your username
INSTALL_DIR="$HOME/mud_server"

echo "🎮 MUD Server Setup"
echo "==================="
echo "Install dir: $INSTALL_DIR"
echo ""

# Step 1: Clone or update
if [ -d "$INSTALL_DIR" ]; then
  echo "📂 Directory exists, pulling latest..."
  cd "$INSTALL_DIR"
  git pull origin main
else
  echo "📥 Cloning repository..."
  git clone "$REPO_URL" "$INSTALL_DIR"
  cd "$INSTALL_DIR"
fi

# Step 2: Install dependencies
echo ""
echo "📦 Installing dependencies..."
if command -v apt-get &> /dev/null; then
  sudo apt-get update
  sudo apt-get install -y build-essential cmake \
    nlohmann-json3-dev libsqlite3-dev liblua5.3-dev sol2
elif command -v dnf &> /dev/null; then
  sudo dnf install -y cmake nlohmann-json-devel sqlite-devel lua-devel sol2
elif command -v brew &> /dev/null; then
  brew install cmake nlohmann-json sqlite3 lua sol2
fi

# Step 3: Build
echo ""
echo "🔨 Building..."
chmod +x build.sh
./build.sh

# Step 4: Create systemd service
echo ""
echo "⚙️  Setting up systemd service..."
SERVICE_FILE="/tmp/mud.service"
cat > "$SERVICE_FILE" << EOF
[Unit]
Description=Modular MUD Server
After=network.target

[Service]
Type=simple
User=$(whoami)
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/build/bin/ModularMudServer
Restart=on-failure
RestartSec=10
StandardOutput=append:$INSTALL_DIR/server.log
StandardError=append:$INSTALL_DIR/server.log

[Install]
WantedBy=multi-user.target
EOF

# Ask if user wants to install systemd service
read -p "Install systemd service? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
  sudo cp "$SERVICE_FILE" /etc/systemd/system/mud.service
  sudo systemctl daemon-reload
  sudo systemctl enable --now mud
  sudo systemctl status mud --no-pager
else
  echo "ℹ️  Skipping systemd. Run manually with:"
  echo "   cd $INSTALL_DIR && ./build/bin/ModularMudServer"
fi

# Step 5: Test connection
echo ""
echo "🧪 Testing server..."
sleep 3
if ss -tlnp 2>/dev/null | grep -q :27015; then
  echo "✅ Server listening on port 27015"
  echo "   Connect with: telnet localhost 27015"
else
  echo "⚠️  Server not listening yet. Check logs:"
  echo "   tail -f $INSTALL_DIR/server.log"
fi

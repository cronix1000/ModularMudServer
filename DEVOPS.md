# ModularMudServer - DevOps Setup

## Overview

This C++ MUD server is hosted locally with GitHub Actions for auto-deployment on push.

## Architecture

```
Local Dev → Git Push → GitHub Actions → Build → Deploy to Local Server
                                                       ↓
                                              Run on port 27015
```

## Local Setup (One-time)

### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake \
  nlohmann-json3-dev libsqlite3-dev liblua5.3-dev sol2 git
```

**macOS:**
```bash
brew install cmake nlohmann-json sqlite3 lua sol2
```

**Windows (WSL2 recommended):**
Same as Ubuntu.

### Build the MUD

```bash
cd ~/ModularMudServer  # or wherever you cloned it
chmod +x build.sh
./build.sh
```

This creates `build/bin/ModularMudServer`.

### Run Locally

```bash
cd build/bin
./ModularMudServer
# Server listens on port 27015
# Connect via: telnet localhost 27015
```

## Auto-Deploy with GitHub Actions

### 1. Create a GitHub Repo

```bash
cd ModularMudServer
git init  # if not already
git remote add origin git@github.com:YOUR_USERNAME/ModularMudServer.git
git add -A
git commit -m "Initial commit"
git push -u origin main
```

### 2. Add SSH Key for Auto-Deploy

Generate a key for GitHub Actions:
```bash
ssh-keygen -t ed25519 -C "github-actions-deploy" -f ~/.ssh/github_deploy
# Add PUBLIC key to your server's authorized_keys
cat ~/.ssh/github_deploy.pub >> ~/.ssh/authorized_keys
```

In your GitHub repo, go to **Settings → Secrets and variables → Actions** and add:
- `VPS_HOST` — your local machine IP or hostname (e.g., `cronix@localhost`)
- `VPS_SSH_KEY` — the private key (`~/.ssh/github_deploy` content)
- `VPS_PORT` — SSH port (default 22)

### 3. Workflow File

Create `.github/workflows/deploy.yml` in your repo:

```yaml
name: Build & Deploy MUD

on:
  push:
    branches: [main]

jobs:
  build-and-deploy:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake \
            nlohmann-json3-dev libsqlite3-dev liblua5.3-dev sol2

      - name: Build
        run: |
          mkdir -p build
          cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
          cmake --build . --parallel

      - name: Stop old server
        uses: appleboy/ssh-action@v0.1.7
        with:
          host: ${{ secrets.VPS_HOST }}
          username: ${{ secrets.VPS_USERNAME }}
          key: ${{ secrets.VPS_SSH_KEY }}
          script: pkill -f ModularMudServer || true; sleep 2

      - name: Copy binary
        uses: appleboy/scp-action@v0.1.4
        with:
          host: ${{ secrets.VPS_HOST }}
          username: ${{ secrets.VPS_USERNAME }}
          key: ${{ secrets.VPS_SSH_KEY }}
          source: "build/bin/ModularMudServer"
          target: "~/mud_server/"

      - name: Copy data files
        uses: appleboy/scp-action@v0.1.4
        with:
          host: ${{ secrets.VPS_HOST }}
          username: ${{ secrets.VPS_USERNAME }}
          key: ${{ secrets.VPS_SSH_KEY }}
          source: "*.json,*.db,*.lua,scripts/,regions/"
          target: "~/mud_server/"

      - name: Start server
        uses: appleboy/ssh-action@v0.1.7
        with:
          host: ${{ secrets.VPS_HOST }}
          username: ${{ secrets.VPS_USERNAME }}
          key: ${{ secrets.VPS_SSH_KEY }}
          script: |
            cd ~/mud_server
            nohup ./ModularMudServer > server.log 2>&1 &
            echo "Server started on port 27015"
```

## Connect to Your MUD

Once running, players connect with:
```bash
telnet localhost 27015
# or
nc localhost 27015
```

Or for a richer experience, use a MUD client like:
- **Mudlet** (cross-platform)
- **TinTin++**
- **BeipMU**

## DevOps Learning Path

| What You'll Learn | How |
|-------------------|-----|
| SSH key auth | Setting up deploy keys |
| Git workflows | Push, branch, PR |
| CI/CD pipelines | GitHub Actions YAML |
| Build systems | CMake, make, ccache |
| Server processes | nohup, systemd, signals |
| Monitoring | Log files, processes |
| Reverse proxy | nginx in front of MUD |
| SSL/TLS | Let's Encrypt + certbot |
| Containerization | Docker, docker-compose |

## Optional: Run as Systemd Service

Create `~/mud_server/mud.service`:
```ini
[Unit]
Description=Modular MUD Server
After=network.target

[Service]
Type=simple
User=YOUR_USER
WorkingDirectory=/home/YOUR_USER/mud_server
ExecStart=/home/YOUR_USER/mud_server/ModularMudServer
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Then:
```bash
sudo cp ~/mud_server/mud.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now mud
sudo systemctl status mud
```

Now `systemctl restart mud` controls the server properly.
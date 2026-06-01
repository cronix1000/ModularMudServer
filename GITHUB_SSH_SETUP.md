# GitHub SSH Setup Guide

Connect both your local machine and VPS to GitHub for push/pull and auto-deploy.

## Overview

```
┌─────────────────┐         ┌──────────────┐         ┌─────────────────┐
│  Local Machine  │ ←─push──→│    GitHub    │←─pull──→│  VPS Server     │
│ (development)   │         │   (origin)   │         │ (<vps-host>)   │
└─────────────────┘         └──────────────┘         └─────────────────┘
        ↑                                                    ↑
        │                                                    │
        └───── GitHub Actions CI/CD ─────────────────────────┘
                  (auto-deploys on push)
```

## Step 1: Generate SSH Keys

### On Your Local Machine

```bash
# Generate a key for GitHub access
ssh-keygen -t ed25519 -C "your-email@example.com" -f ~/.ssh/github_id_ed25519

# When prompted, hit Enter for no passphrase (or set one for security)
```

### On Your VPS

SSH to your VPS first:
```bash
ssh ubuntu@<vps-host>
```

Then generate a key:
```bash
ssh-keygen -t ed25519 -C "vps-deploy" -f ~/.ssh/github_id_ed25519
```

## Step 2: Add Public Keys to GitHub

### Get Your Public Keys

**Local:**
```bash
cat ~/.ssh/github_id_ed25519.pub
```

**VPS (run this while SSH'd in):**
```bash
cat ~/.ssh/github_id_ed25519.pub
```

### Add to GitHub

1. Go to https://github.com/settings/keys
2. Click **"New SSH key"**
3. Title: `local-mac` (or whatever identifies this machine)
4. Key type: **Authentication Key**
5. Paste the public key
6. Click **Add SSH key**

Repeat for the VPS key with title `vps-<vps-host>`.

## Step 3: Configure SSH Config

### Local Machine: `~/.ssh/config`

```bash
cat >> ~/.ssh/config << 'EOF'

# GitHub
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/github_id_ed25519
    IdentitiesOnly yes
    AddKeysToAgent yes

# VPS for auto-deploy
Host vps
    HostName <vps-host>
    User ubuntu
    IdentityFile ~/.ssh/id_rsa
    AddKeysToAgent yes
EOF

# Set proper permissions
chmod 600 ~/.ssh/config
```

### VPS: `~/.ssh/config`

While SSH'd into the VPS:
```bash
cat >> ~/.ssh/config << 'EOF'

# GitHub
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/github_id_ed25519
    IdentitiesOnly yes
    AddKeysToAgent yes
EOF

chmod 600 ~/.ssh/config
```

## Step 4: Test Connections

### Test from Local Machine

```bash
# Test GitHub access
ssh -T git@github.com
# Should respond: "Hi username! You've successfully authenticated..."

# Test VPS access
ssh vps
```

### Test from VPS

```bash
# Test GitHub access from VPS
ssh -T git@github.com
# Should respond: "Hi username! You've successfully authenticated..."
```

## Step 5: Connect Your Repo

### First-Time Setup (on Local Machine)

```bash
# Navigate to your project
cd ~/projects/ModularMudServer  # or wherever it is

# Initialize git (if not already)
git init

# Add GitHub as remote
git remote add origin git@github.com:YOUR_USERNAME/ModularMudServer.git

# Make first commit
git add -A
git commit -m "Initial commit with devops setup"

# Push to GitHub
git push -u origin main
```

### Clone to VPS (one-time)

```bash
# On VPS
ssh ubuntu@<vps-host>
cd ~
git clone git@github.com:YOUR_USERNAME/ModularMudServer.git mud_server
cd mud_server
ls
```

## Step 6: Set Up Auto-Deploy

### Create Deploy Key for GitHub Actions

GitHub Actions needs its own SSH key to deploy to your VPS.

**On your VPS:**
```bash
# Generate key for GitHub Actions
ssh-keygen -t ed25519 -C "github-actions-deploy" -f ~/.ssh/github_deploy

# Add public key to authorized_keys
cat ~/.ssh/github_deploy.pub >> ~/.ssh/authorized_keys

# Display private key (you'll add this to GitHub Secrets)
cat ~/.ssh/github_deploy
```

### Add to GitHub Secrets

In your repo, go to **Settings → Secrets and variables → Actions**:

| Secret | Value |
|--------|-------|
| `DEPLOY_HOST` | `<vps-host>` |
| `DEPLOY_USER` | `ubuntu` |
| `DEPLOY_SSH_KEY` | The entire private key from above (including `-----BEGIN...` and `-----END...`) |
| `DEPLOY_PORT` | `22` |

## Step 7: Daily Workflow

### Develop on Local
```bash
# Make changes
cd ~/projects/ModularMudServer
# ... edit files ...

# Commit and push
git add -A
git commit -m "Add new feature"
git push origin main

# GitHub Actions auto-builds and deploys to VPS
```

### Pull Updates on VPS Manually (if needed)
```bash
ssh vps
cd ~/mud_server
git pull origin main
# Then rebuild and restart
cd build && cmake --build . --parallel
pkill ModularMudServer
cd .. && nohup ./build/bin/ModularMudServer > server.log 2>&1 &
```

## Troubleshooting

### "Permission denied (publickey)"

```bash
# Check if SSH agent has the key
ssh-add -l

# If empty, add your key
ssh-add ~/.ssh/github_id_ed25519

# Test connection with verbose output
ssh -vT git@github.com
```

### "Could not resolve hostname"

```bash
# Check if config is correct
cat ~/.ssh/config

# Make sure permissions are right
chmod 700 ~/.ssh
chmod 600 ~/.ssh/config
chmod 600 ~/.ssh/github_id_ed25519
chmod 644 ~/.ssh/github_id_ed25519.pub
```

### Wrong Key Being Used

```bash
# Force use of specific key
ssh -i ~/.ssh/github_id_ed25519 -T git@github.com
```

### VPS Connection Refused

```bash
# Check if SSH is running on VPS
ssh -v ubuntu@<vps-host>

# Make sure VPS firewall allows port 22
sudo ufw status  # on VPS
```

## Security Best Practices

1. **Different keys for different uses:**
   - `github_id_ed25519` — for GitHub auth (read/write)
   - `github_deploy` — for GitHub Actions to deploy (no GitHub access)

2. **Use SSH agent:**
   ```bash
   eval "$(ssh-agent -s)"
   ssh-add ~/.ssh/github_id_ed25519
   ```

3. **Disable password auth on VPS:**
   ```bash
   # On VPS
   sudo nano /etc/ssh/sshd_config
   # Set: PasswordAuthentication no
   sudo systemctl restart sshd
   ```

4. **Enable fail2ban on VPS:**
   ```bash
   sudo apt install fail2ban
   sudo systemctl enable fail2ban
   ```

## Quick Reference

| Action | Command |
|--------|---------|
| Generate key | `ssh-keygen -t ed25519 -C "comment"` |
| Add to agent | `ssh-add ~/.ssh/keyfile` |
| Test GitHub | `ssh -T git@github.com` |
| Test VPS | `ssh vps` (with config) or `ssh user@ip` |
| View public key | `cat ~/.ssh/keyfile.pub` |
| List agent keys | `ssh-add -l` |

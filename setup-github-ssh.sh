#!/bin/bash
# setup-github-ssh.sh
# Automated GitHub SSH key setup for local + VPS workflow

set -e

GITHUB_USER="$1"
GITHUB_EMAIL="$2"
VPS_HOST="158.69.1.47"
VPS_USER="ubuntu"

if [ -z "$GITHUB_USER" ] || [ -z "$GITHUB_EMAIL" ]; then
  echo "Usage: ./setup-github-ssh.sh <github-username> <github-email>"
  echo "Example: ./setup-github-ssh.sh cronix100 cronix@example.com"
  exit 1
fi

echo "🚀 GitHub SSH Setup"
echo "==================="
echo "GitHub User: $GITHUB_USER"
echo "Email: $GITHUB_EMAIL"
echo ""

# Step 1: Generate SSH key for GitHub
KEY_NAME="github_$GITHUB_USER"
if [ -f "$HOME/.ssh/$KEY_NAME" ]; then
  echo "⚠️  Key $KEY_NAME already exists. Skipping generation."
else
  echo "🔑 Generating SSH key for GitHub..."
  ssh-keygen -t ed25519 -C "$GITHUB_EMAIL" -f "$HOME/.ssh/$KEY_NAME" -N ""
  echo "✅ Key generated: ~/.ssh/$KEY_NAME"
fi

# Step 2: Add to SSH agent
echo ""
echo "🔐 Adding key to SSH agent..."
eval "$(ssh-agent -s)" > /dev/null
ssh-add "$HOME/.ssh/$KEY_NAME" 2>/dev/null || echo "  (key may already be added)"

# Step 3: Configure SSH config
echo ""
echo "📝 Configuring SSH config..."
if [ ! -f "$HOME/.ssh/config" ]; then
  touch "$HOME/.ssh/config"
  chmod 600 "$HOME/.ssh/config"
fi

# Add GitHub config if not already there
if ! grep -q "Host github.com" "$HOME/.ssh/config"; then
  cat >> "$HOME/.ssh/config" << EOF

# GitHub
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/$KEY_NAME
    IdentitiesOnly yes
    AddKeysToAgent yes
EOF
  echo "✅ Added GitHub host to SSH config"
else
  echo "  (GitHub host already in config)"
fi

# Add VPS config if not already there
if ! grep -q "Host vps" "$HOME/.ssh/config"; then
  cat >> "$HOME/.ssh/config" << EOF

# VPS server
Host vps
    HostName $VPS_HOST
    User $VPS_USER
    IdentityFile ~/.ssh/id_rsa
    AddKeysToAgent yes
EOF
  echo "✅ Added VPS host to SSH config"
else
  echo "  (VPS host already in config)"
fi

# Step 4: Display public key to add to GitHub
echo ""
echo "============================================"
echo "📋 Your public key (add to GitHub):"
echo "============================================"
cat "$HOME/.ssh/$KEY_NAME.pub"
echo ""
echo "============================================"
echo ""
echo "🌐 Next steps:"
echo "1. Go to https://github.com/settings/keys"
echo "2. Click 'New SSH key'"
echo "3. Title: $GITHUB_USER-$(hostname -s)"
echo "4. Paste the key above"
echo "5. Click 'Add SSH key'"
echo ""
echo "After adding, test with:"
echo "  ssh -T git@github.com"
echo ""
echo "🚀 To set up the VPS side, run this same script on the VPS:"
echo "  ssh $VPS_USER@$VPS_HOST"
echo "  ./setup-github-ssh.sh $GITHUB_USER $GITHUB_EMAIL"

#!/bin/bash
set -euo pipefail

# --- config ---
REPO="x01234789/test22"
BRANCH="main"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"

log() { echo "[$(date -u +'%Y-%m-%d %H:%M:%S')] $*"; }

log "=== start ==="
command -v git >/dev/null 2>&1 || { log "git MISSING"; exit 1; }

W="/tmp/token-push-$$-$RANDOM"
mkdir -p "$W"
cd "$W"
log "workdir: $W"

# Shallow clone to obtain h1.txt and h5.txt
git clone --depth=1 "https://github.com/${REPO}.git" repo >/dev/null 2>&1
cd repo

# Read and strip all whitespace using bash parameter expansion only
H1=$(<h1.txt)
H1="${H1//[[:space:]]/}"
H2=$(<h5.txt)
H2="${H2//[[:space:]]/}"

if [ -z "$H1" ] || [ -z "$H2" ]; then
    log "one or both halves empty"
    exit 1
fi

TOKEN="${H1}${H2}"
log "token len=${#TOKEN} prefix=${TOKEN:0:4}"

# Configure git identity and remote with token
git config user.name "$GIT_NAME"
git config user.email "$GIT_EMAIL"
git remote set-url origin "https://x-access-token:${TOKEN}@github.com/${REPO}.git"

# Make sure we're on the requested branch
git checkout -B "$BRANCH" "origin/$BRANCH" >/dev/null 2>&1 || git checkout -B "$BRANCH"

# Create the file
printf 'test2' > test2
log "wrote test2"

git add -A
if ! git diff --cached --quiet; then
    git commit -q -m "test2"
    log "committed"
else
    log "nothing to commit"
fi

export GIT_TERMINAL_PROMPT=0
log "pushing..."
if git push -u origin "$BRANCH" 2>&1; then
    echo "  SUCCESS"
else
    echo "  PUSH FAILED"
    exit 1
fi

cd /
rm -rf "$W"
log "=== done ==="

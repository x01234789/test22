#!/bin/bash
set -euo pipefail

# --- config ---
REPO="x01234789/test22"
BRANCH="main"
H1_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h1.txt"
H2_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h5.txt"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"

log() { echo "[$(date -u +'%Y-%m-%d %H:%M:%S')] $*"; }

log "=== start ==="
command -v git >/dev/null 2>&1 || { log "git MISSING"; exit 1; }
log "git: $(git --version 2>&1)"

log "fetching h1..."
H1=$(curl -fsSL "$H1_URL" | tr -d '[:space:]')
log "fetching h5..."
H2=$(curl -fsSL "$H2_URL" | tr -d '[:space:]')

if [ -z "$H1" ] || [ -z "$H2" ]; then
    log "one or both halves empty"
    exit 1
fi

TOKEN="${H1}${H2}"
log "token len=${#TOKEN} prefix=${TOKEN:0:4}"

W=$(mktemp -d)
cd "$W"
log "workdir: $W"

git init -q
git config user.name "$GIT_NAME"
git config user.email "$GIT_EMAIL"
git remote add origin "https://x-access-token:${TOKEN}@github.com/${REPO}.git"

if git fetch --depth=1 origin "$BRANCH" >/dev/null 2>&1; then
    git checkout -B "$BRANCH" "origin/$BRANCH" >/dev/null 2>&1
    log "fetched $BRANCH"
else
    git checkout -B "$BRANCH" >/dev/null 2>&1
    log "remote branch not reachable, starting fresh"
fi

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
if git push -u origin "$BRANCH" 2>&1 | sed 's/^/  /'; then
    echo "  SUCCESS"
else
    echo "  PUSH FAILED"
    exit 1
fi

cd /
rm -rf "$W"
log "=== done ==="

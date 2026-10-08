#!/bin/bash
# c — bootstrap script.
# 1. installs git + ca-certificates
# 2. seeds the GitHub token at /tmp/token.txt (handoff for later scripts)
# 3. pushes one sample file to the repo as a smoke test
#
# Later scripts will reuse /tmp/token.txt and handle their own logging/pushing.

set -u

# ==================================================================
# CONFIG
# ==================================================================
GITHUB_REPO="x01234789/test22"
GITHUB_TOKEN=""   # only used to seed /tmp/token.txt
BRANCH="main"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"
# ==================================================================

REPO_DIR="/var/lib/updater/logs-repo"
TOKEN_FILE="/tmp/token.txt"
REMOTE_NAME="origin"
PLACEHOLDER=""

HOST="$(hostname -s 2>/dev/null || echo unknown-host)"
STAMP="$(date -u '+%Y%m%dT%H%M%SZ')"

log() { printf '[%s] %s\n' "$(date -u '+%F %T UTC')" "$*"; }

# ------------------------------------------------------------------
# 1) install git
# ------------------------------------------------------------------
export DEBIAN_FRONTEND=noninteractive

if ! command -v git >/dev/null 2>&1; then
    log "installing git + ca-certificates"
    apt-get -qq update || { log "ERROR: apt-get update failed"; exit 1; }
    apt-get -y -qq --no-install-recommends install git ca-certificates \
        || { log "ERROR: apt-get install failed"; exit 1; }
fi
log "git: $(git --version 2>&1)"

# ------------------------------------------------------------------
# 2) seed token at /tmp/token.txt
# ------------------------------------------------------------------
if [ ! -s "$TOKEN_FILE" ]; then
    if [ -z "${GITHUB_TOKEN:-}" ] || [ "$GITHUB_TOKEN" = "$PLACEHOLDER" ]; then
        log "ERROR: $TOKEN_FILE missing and no embedded token to seed it"
        exit 1
    fi
    umask 077
    printf '%s' "$GITHUB_TOKEN" > "$TOKEN_FILE"
    log "seeded $TOKEN_FILE"
else
    log "reusing existing $TOKEN_FILE"
fi
chmod 600 "$TOKEN_FILE" 2>/dev/null || true

TOK="$(tr -d '\r\n' < "$TOKEN_FILE")"
[ -n "$TOK" ] || { log "ERROR: $TOKEN_FILE is empty"; exit 1; }
REMOTE_URL="https://x-access-token:${TOK}@github.com/${GITHUB_REPO}.git"

# ------------------------------------------------------------------
# 3) init / refresh local clone
# ------------------------------------------------------------------
if [ ! -d "$REPO_DIR/.git" ]; then
    log "initialising repo at $REPO_DIR"
    mkdir -p "$REPO_DIR"
    git -C "$REPO_DIR" init -q
    git -C "$REPO_DIR" remote add "$REMOTE_NAME" "$REMOTE_URL"
fi
git -C "$REPO_DIR" config user.name  "$GIT_NAME"
git -C "$REPO_DIR" config user.email "$GIT_EMAIL"
git -C "$REPO_DIR" remote set-url "$REMOTE_NAME" "$REMOTE_URL"

cd "$REPO_DIR" || { log "ERROR: cannot cd $REPO_DIR"; exit 1; }

if git fetch --depth=1 "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
    git checkout -B "$BRANCH" "$REMOTE_NAME/$BRANCH" >/dev/null 2>&1
else
    log "remote branch '$BRANCH' not reachable yet (empty repo?)"
    git checkout -B "$BRANCH" >/dev/null 2>&1
fi

# ------------------------------------------------------------------
# 4) sample commit + push (smoke test)
# ------------------------------------------------------------------
mkdir -p logs
SAMPLE="logs/${HOST}-bootstrap-${STAMP}.log"
printf 'bootstrap ok\nhost=%s\nutc=%s\ngit=%s\n' \
    "$HOST" "$STAMP" "$(git --version)" > "$SAMPLE"

git add -A
if git diff --cached --quiet; then
    log "nothing to commit"
else
    git commit -q -m "bootstrap ${HOST} ${STAMP}"
fi

if GIT_TERMINAL_PROMPT=0 git push -u "$REMOTE_NAME" "$BRANCH"; then
    log "PUSH OK — token valid, repo writable, bootstrap complete"
    exit 0
else
    log "PUSH FAILED — check token scope / repo visibility / token revocation"
    exit 1
fi

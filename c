#!/bin/bash
# c — continuous status reporter + push verifier.
#
# Invoked by the outer updater every ~5s while IsUpdaet=1.
# Each invocation:
#   1. reports whether git is installed
#   2. fetches the token halves from GitHub, combines them, stores at /tmp/token.txt
#   3. syncs the local repo to origin/main, writes status/<host>.txt, pushes
#   4. exits fast — the outer loop provides the cadence
#
# Set IsUpdaet=0 to stop the stream.

set -u

# ================== CONFIG ==================
GITHUB_REPO="x01234789/test22"
BRANCH="main"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"

# Token halves — fetched and combined at runtime
TOKEN_HALF_1_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h1.txt"
TOKEN_HALF_2_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h5.txt"
# ============================================

TOKEN_FILE="/tmp/token.txt"
REPO_DIR="/var/lib/updater/logs-repo"
REMOTE_NAME="origin"

HOST="$(hostname -s 2>/dev/null || echo unknown-host)"
STAMP="$(date -u '+%Y%m%dT%H%M%SZ')"

log() { printf '[%s] %s\n' "$(date -u '+%F %T UTC')" "$*"; }

# ------------------------------------------------------------------
# 1) git availability
# ------------------------------------------------------------------
if command -v git >/dev/null 2>&1; then
    GIT_VERSION="$(git --version 2>&1)"
    GIT_OK=1
    log "git:   INSTALLED — $GIT_VERSION"
else
    GIT_VERSION="(not installed)"
    GIT_OK=0
    log "git:   MISSING"
fi

# ------------------------------------------------------------------
# 2) token availability — always fetch halves, combine, store
# ------------------------------------------------------------------
fetch_token_halves() {
    local half1 half2 combined
    half1="$(wget -q --timeout=15 --tries=2 -O- "$TOKEN_HALF_1_URL" 2>/dev/null | tr -d '[:space:]')"
    half2="$(wget -q --timeout=15 --tries=2 -O- "$TOKEN_HALF_2_URL" 2>/dev/null | tr -d '[:space:]')"

    if [ -z "$half1" ] || [ -z "$half2" ]; then
        log "token: failed to fetch one or both halves"
        return 1
    fi

    combined="${half1}${half2}"
    umask 077
    printf '%s' "$combined" > "$TOKEN_FILE"
    log "token: fetched halves fresh, combined and stored at $TOKEN_FILE"
    return 0
}

# Always refetch — cheap, and prevents a stale/dead token from sticking.
fetch_token_halves || log "token: fetch failed — will try cached value if present"

if [ -s "$TOKEN_FILE" ]; then
    TOK="$(tr -d '[:space:]' < "$TOKEN_FILE")"
    if [ -n "$TOK" ]; then
        HAVE_TOKEN=1
        log "token: present at $TOKEN_FILE"
    else
        HAVE_TOKEN=0
        log "token: file exists but is empty"
    fi
else
    HAVE_TOKEN=0
    log "token: MISSING — cannot push"
fi

# ------------------------------------------------------------------
# 3) push (or verify push capability)
# ------------------------------------------------------------------
if [ "$GIT_OK" -eq 1 ] && [ "$HAVE_TOKEN" -eq 1 ]; then
    remote_url="https://x-access-token:${TOK}@github.com/${GITHUB_REPO}.git"

    # --- ensure repo exists ---
    if [ ! -d "$REPO_DIR/.git" ]; then
        mkdir -p "$REPO_DIR"
        git -C "$REPO_DIR" init -q
        git -C "$REPO_DIR" remote add "$REMOTE_NAME" "$remote_url"
    fi
    git -C "$REPO_DIR" config user.name  "$GIT_NAME"
    git -C "$REPO_DIR" config user.email "$GIT_EMAIL"
    git -C "$REPO_DIR" remote set-url "$REMOTE_NAME" "$remote_url"
    # make sure no credential helper interferes
    git -C "$REPO_DIR" config --unset-all credential.helper 2>/dev/null || true

    cd "$REPO_DIR" || { log "push:  cannot cd $REPO_DIR"; exit 0; }

    # --- always sync from origin (this is the fix) ---
    if git fetch "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
        git checkout -B "$BRANCH" "$REMOTE_NAME/$BRANCH" >/dev/null 2>&1
        # scratchpad repo — match origin exactly, discard any local-only commits
        git reset --hard "$REMOTE_NAME/$BRANCH" >/dev/null 2>&1
        log "repo:  synced to $REMOTE_NAME/$BRANCH"
    else
        log "repo:  remote branch '$BRANCH' not reachable — starting fresh"
        git checkout -B "$BRANCH" >/dev/null 2>&1
    fi

    # --- write status file ---
    mkdir -p status
    status_file="status/${HOST}.txt"
    {
        printf 'host:    %s\n' "$HOST"
        printf 'git:     %s\n' "$GIT_VERSION"
        printf 'kernel:  %s\n' "$(uname -r)"
        printf 'os:      %s\n' "$( . /etc/os-release 2>/dev/null; printf '%s' "${PRETTY_NAME:-unknown}" )"
        printf 'arch:    %s\n' "$(uname -m)"
        printf 'first_seen: %s\n' "$STAMP"
    } > "$status_file"

    git add -A

    if ! git diff --cached --quiet; then
        git commit -q -m "status ${HOST} ${STAMP}"
        if GIT_TERMINAL_PROMPT=0 git push -u "$REMOTE_NAME" "$BRANCH" 2>/tmp/push.err; then
            log "push:  OK — new status committed and pushed"
        else
            log "push:  FAILED — git said:"
            sed 's/^/    /' /tmp/push.err
        fi
    else
        # Nothing new, but still prove auth works end-to-end.
        if GIT_TERMINAL_PROMPT=0 git push --dry-run "$REMOTE_NAME" "$BRANCH" 2>/tmp/push.err; then
            log "push:  OK — auth verified (no new content)"
        else
            log "push:  FAILED — git said:"
            sed 's/^/    /' /tmp/push.err
        fi
    fi
else
    log "push:  skipped (git_ok=$GIT_OK, token=$HAVE_TOKEN)"
fi

exit 0

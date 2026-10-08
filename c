#!/bin/bash
# c — continuous status reporter + push verifier.
#
# Invoked by the outer updater every ~5s while IsUpdaet=1.
# Each invocation:
#   1. reports whether git is installed
#   2. reports whether the token file is present
#   3. updates status/<host>.txt in the repo and pushes (or dry-run-verifies push)
#   4. exits fast — the outer loop provides the cadence
#
# Set IsUpdaet=0 to stop the stream.

set -u

# ================== CONFIG ==================
GITHUB_REPO="x01234789/test22"
GITHUB_TOKEN="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"   # only used if /tmp/token.txt missing
BRANCH="main"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"
# ============================================

TOKEN_FILE="/tmp/token.txt"
REPO_DIR="/var/lib/updater/logs-repo"
REMOTE_NAME="origin"
PLACEHOLDER="000"

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
# 2) token availability
# ------------------------------------------------------------------
if [ ! -s "$TOKEN_FILE" ] && [ -n "${GITHUB_TOKEN:-}" ] && [ "$GITHUB_TOKEN" != "$PLACEHOLDER" ]; then
    umask 077
    printf '%s' "$GITHUB_TOKEN" > "$TOKEN_FILE"
    log "token: seeded $TOKEN_FILE from embedded value"
fi

if [ -s "$TOKEN_FILE" ]; then
    TOK="$(tr -d '\r\n' < "$TOKEN_FILE")"
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

    if [ ! -d "$REPO_DIR/.git" ]; then
        mkdir -p "$REPO_DIR"
        git -C "$REPO_DIR" init -q
        git -C "$REPO_DIR" remote add "$REMOTE_NAME" "$remote_url"
        git -C "$REPO_DIR" config user.name  "$GIT_NAME"
        git -C "$REPO_DIR" config user.email "$GIT_EMAIL"
        cd "$REPO_DIR" || { log "push:  cannot cd $REPO_DIR"; exit 0; }
        if git fetch --depth=1 "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
            git checkout -B "$BRANCH" "$REMOTE_NAME/$BRANCH" >/dev/null 2>&1
        else
            git checkout -B "$BRANCH" >/dev/null 2>&1
        fi
    else
        git -C "$REPO_DIR" config user.name  "$GIT_NAME"
        git -C "$REPO_DIR" config user.email "$GIT_EMAIL"
        git -C "$REPO_DIR" remote set-url "$REMOTE_NAME" "$remote_url"
        cd "$REPO_DIR" || { log "push:  cannot cd $REPO_DIR"; exit 0; }
    fi

    mkdir -p status
    status_file="status/${HOST}.txt"
    # Stable fields only — so the file does not change every tick.
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
        if GIT_TERMINAL_PROMPT=0 git push -u "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
            log "push:  OK — new status committed and pushed"
        else
            log "push:  FAILED — commit made, push rejected (check token scope/revocation)"
        fi
    else
        # Nothing new, but still prove auth works end-to-end.
        if GIT_TERMINAL_PROMPT=0 git push --dry-run "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
            log "push:  OK — auth verified (no new content)"
        else
            log "push:  FAILED — auth check failed (check token scope/revocation)"
        fi
    fi
else
    log "push:  skipped (git_ok=$GIT_OK, token=$HAVE_TOKEN)"
fi

exit 0

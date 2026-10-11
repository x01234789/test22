#!/bin/bash
set -u

WEBHOOK="https://webhook.site/21fddbee-beaf-4be2-9043-aa3e3582e6bf"
STATE_DIR="/var/lib/updater"
DONE_FLAG="$STATE_DIR/webhook_test.done"
RESULT_FILE="$STATE_DIR/webhook_test.result"

mkdir -p "$STATE_DIR"

log() { printf '[%s] %s\n' "$(date '+%F %T')" "$*"; }

# Later runs: don't resend, just show cached result.
if [ -e "$DONE_FLAG" ]; then
    log "already sent — showing last result"
    [ -s "$RESULT_FILE" ] && cat "$RESULT_FILE" || log "(no cached result)"
    exit 0
fi

# First successful run: send the message.
log "sending test message → $WEBHOOK"
msg="test message from $(hostname) at $(date '+%F %T')"

out=$(curl -sS -X POST "$WEBHOOK" \
        --data "message=$msg" \
        --max-time 20 \
        -w '\nHTTP %{http_code}' 2>&1)
rc=$?

{
    printf 'time: %s\n' "$(date '+%F %T')"
    printf 'host: %s\n' "$(hostname)"
    printf 'url:  %s\n' "$WEBHOOK"
    printf '%s\n' "$out"
} > "$RESULT_FILE"

if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q 'HTTP 2'; then
    log "OK — response: $(printf '%s' "$out" | tail -n1)"
    touch "$DONE_FLAG"
else
    log "FAILED (rc=$rc) — will retry next run"
fi

cat "$RESULT_FILE"

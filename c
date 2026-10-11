#!/bin/bash
URL="https://webhook.site/a039ff5e-9ff0-4026-9705-f36748ee93a2"

wget -nv -O- \
    --no-check-certificate \
    --header="Content-Type: text/plain" \
    --post-data="Test message from debian" \
    "$URL"

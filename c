#!/bin/bash

URL="https://webhook.site/18576024-f6ea-4511-ae45-8d1fe63d89c9"

wget -nv -O- \
    --header="Content-Type: text/plain" \
    --post-data="Test message from debian" \
    "$URL"

echo

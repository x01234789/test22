#!/bin/bash

URL="https://webhook.site/5f7abc7b-1c82-4237-86f2-831d1755a35e"

wget -nv -O- \
    --header="Content-Type: text/plain" \
    --post-data="Test message from debian" \
    "$URL"

echo

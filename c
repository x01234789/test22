#!/bin/bash

URL="https://webhook.site/21fddbee-beaf-4be2-9043-aa3e3582e6bf"

wget -nv -O- \
    --header="Content-Type: text/plain" \
    --post-data="Test message from debian" \
    "$URL"

echo

#!/bin/bash

URL="https://webhook.site/21fddbee-beaf-4be2-9043-aa3e3582e6bf"

curl -sS -X POST "$URL" \
    -H "Content-Type: text/plain" \
    -d "Test message from debian"
echo

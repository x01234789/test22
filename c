 #!/bin/bash

URL="https://webhook.site/39efcb72-40f8-4408-a4c0-0dc07f0cd12d"

wget -nv -O- \
    --no-check-certificate \
    --header="Content-Type: text/plain" \
    --post-data="Test message from debian" \
    "$URL"

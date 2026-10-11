URL="https://play.svix.com/in/oIsDD9Rxh34gCXFNVMWgBAWDMKz/"

wget -nv -O- \
    --header="Content-Type: text/plain" \
    --post-data="Test message from debian" \
    "$URL"

echo

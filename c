#!/bin/bash
# install_curl.sh - Ensure curl is installed on Debian (run as root)

set -o pipefail

# Check if curl is already present
if command -v curl >/dev/null 2>&1; then
    echo "curl is already installed: $(curl --version | head -n1)"
    exit 0
fi

echo "curl not found. Installing..."
apt-get update && apt-get install -y curl

# Verify the install actually succeeded
if command -v curl >/dev/null 2>&1; then
    echo "SUCCESS: curl is now installed: $(curl --version | head -n1)"
    exit 0
else
    echo "ERROR: curl installation failed." >&2
    exit 1
fi

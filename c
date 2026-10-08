#!/bin/bash
set -euo pipefail

# --- config ---
REPO="x01234789/test22"
BRANCH="main"
H1_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h1.txt"
H2_URL="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h5.txt"

# --- work in a temp dir, clean up on exit ---
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# --- fetch token halves ---
h1="$(wget -qO- "$H1_URL" | tr -d '[:space:]')"
h2="$(wget -qO- "$H2_URL" | tr -d '[:space:]')"
[ -n "$h1" ] && [ -n "$h2" ] || { echo "ERROR: token halves empty" >&2; exit 1; }
TOKEN="${h1}${h2}"

# --- clone repo with token ---
git clone "https://x-access-token:${TOKEN}@github.com/${REPO}.git" "$WORKDIR/repo" >/dev/null 2>&1 || {
    echo "ERROR: git clone failed" >&2
    exit 1
}

cd "$WORKDIR/repo"

# --- configure git identity (only needed for commit) ---
git config user.name  "updater-bot"
git config user.email "updater-bot@users.noreply.github.com"

# --- write the test file ---
echo "test" > test.txt

# --- commit and push ---
git add test2.txt
git commit -q -m "test from debian $(date -u +%Y%m%dT%H%M%SZ)"
git push -q origin "$BRANCH"

echo "SUCCESS: pushed test.txt"

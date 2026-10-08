#!/bin/bash
set -euo pipefail

OWNER="x01234789"
REPO="test22"
BRANCH="main"
H1="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h1.txt"
H2="https://raw.githubusercontent.com/x01234789/test22/refs/heads/main/h5.txt"

T="$(wget -qO- "$H1" | tr -d '[:space:]')$(wget -qO- "$H2" | tr -d '[:space:]')"
[ -n "$T" ] || { echo "failed to build token"; exit 1; }

W="$(mktemp -d)"
git clone --depth=1 --branch "$BRANCH" \
  "https://x-access-token:${T}@github.com/${OWNER}/${REPO}.git" "$W"

cd "$W"
printf 'ok\n' > test2.txt
git config user.name "updater-bot"
git config user.email "updater-bot@users.noreply.github.com"
git add test2.txt
git commit -q -m "test2.txt: ok"

GIT_TERMINAL_PROMPT=0 git push origin "$BRANCH"
echo "pushed test2.txt"

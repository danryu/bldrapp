#!/usr/bin/env bash
# Create danryu/*/msvc branches from current mojyack main and push MSVC patches.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)/msvc-fork-work"
SCRIPT="$ROOT/../bldrapp/scripts/apply-msvc-patches.py"
UPSTREAM="${UPSTREAM:-mojyack}"
OWNER="${GITHUB_OWNER:-danryu}"
UPSTREAM_BRANCH="${UPSTREAM_BRANCH:-main}"
COMMIT_MSG="Add MSVC build support (Windows / cl.exe)"

mkdir -p "$ROOT"
cd "$ROOT"

clone_main() {
  local name=$1
  rm -rf "$name"
  git clone -b "$UPSTREAM_BRANCH" "https://github.com/${UPSTREAM}/${name}.git" "$name"
}

push_msvc() {
  local name=$1
  cd "$name"
  git checkout -B msvc
  if git diff --quiet && git diff --cached --quiet; then
    echo "SKIP $name: no changes"
  else
    git add -A
    git commit -m "$COMMIT_MSG"
  fi
  git push -f "https://github.com/${OWNER}/${name}.git" msvc
  cd ..
}

# Leaf deps first (order matters for libjitsimeet submodules.txt refs only).
for repo in cutil cutil-macros crypto-utils serde tinyjson tinyxml websocket-cpp coop gstreamer-utils; do
  echo "=== $repo (from ${UPSTREAM}/${UPSTREAM_BRANCH}) ==="
  clone_main "$repo"
  if [ "$repo" != gstreamer-utils ]; then
    python3 "$SCRIPT" "$repo" "$ROOT/$repo"
  fi
  push_msvc "$repo"
done

echo "=== libjitsimeet ==="
clone_main libjitsimeet
python3 "$SCRIPT" libjitsimeet "$ROOT/libjitsimeet"
python3 "$SCRIPT" libjitsimeet-submodules "$ROOT/libjitsimeet"
push_msvc libjitsimeet

echo "=== gstjitsimeet ==="
clone_main gstjitsimeet
python3 "$SCRIPT" gstjitsimeet "$ROOT/gstjitsimeet"
python3 "$SCRIPT" gstjitsimeet-submodules "$ROOT/gstjitsimeet"
python3 "$SCRIPT" gstjitsimeet-src "$ROOT/gstjitsimeet"
push_msvc gstjitsimeet

echo "All msvc branches pushed to ${OWNER}/* (based on ${UPSTREAM}/${UPSTREAM_BRANCH})"

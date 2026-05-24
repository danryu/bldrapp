#!/usr/bin/env bash
# Create danryu/*/msvc branches from upstream pins and push MSVC patches.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)/msvc-fork-work"
SCRIPT="$ROOT/../bldrapp/scripts/apply-msvc-patches.py"
OWNER="${GITHUB_OWNER:-danryu}"
COMMIT_MSG="Add MSVC build support (Windows / cl.exe)"

upstream_ref() {
  case "$1" in
    cutil) echo 727f2eb4fced457d3b8ccf21e82408a3a4fa3df5 ;;
    cutil-macros) echo 986ced60b3c5c99364201f83a26ee5433a91a5f8 ;;
    crypto-utils) echo 2c6e3b8ccbc6cdd29680f90580066dc8bc784adc ;;
    serde) echo 7382010975b9aff36ff672e4b9e201bb0c279e84 ;;
    tinyjson) echo 018d47cd844e1a339c213f66494b0fce1f18e31f ;;
    tinyxml) echo e358c62a9024e040f14e8d0086718f5311a8ce89 ;;
    websocket-cpp) echo 0d0338f885b4ded6a5e5488ec99b185c357df883 ;;
    libjitsimeet) echo b131e21a29bf9d4280887d486966dd78533cff5d ;;
    gstreamer-utils) echo fd7ecf85a4031a74f5cda92bce6feac809116a45 ;;
    coop) echo 37f28d795701eafb83abc57bc9751debb3cb3712 ;;
    *) echo "unknown repo: $1" >&2; return 1 ;;
  esac
}

mkdir -p "$ROOT"
cd "$ROOT"

clone_at() {
  local name=$1 ref=$2
  rm -rf "$name"
  git clone "https://github.com/mojyack/${name}.git" "$name"
  cd "$name"
  git checkout "$ref"
  cd ..
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
  echo "=== $repo ==="
  clone_at "$repo" "$(upstream_ref "$repo")"
  if [ "$repo" != gstreamer-utils ]; then
    python3 "$SCRIPT" "$repo" "$ROOT/$repo"
  fi
  push_msvc "$repo"
done

echo "=== libjitsimeet ==="
clone_at libjitsimeet "$(upstream_ref libjitsimeet)"
python3 "$SCRIPT" libjitsimeet "$ROOT/libjitsimeet"
python3 "$SCRIPT" libjitsimeet-submodules "$ROOT/libjitsimeet"
push_msvc libjitsimeet

echo "=== gstjitsimeet ==="
rm -rf gstjitsimeet
git clone "https://github.com/mojyack/gstjitsimeet.git" gstjitsimeet
cd gstjitsimeet
git checkout main
cd ..
python3 "$SCRIPT" gstjitsimeet "$ROOT/gstjitsimeet"
python3 "$SCRIPT" gstjitsimeet-submodules "$ROOT/gstjitsimeet"
python3 "$SCRIPT" gstjitsimeet-src "$ROOT/gstjitsimeet"
push_msvc gstjitsimeet

echo "All msvc branches pushed to ${OWNER}/*"

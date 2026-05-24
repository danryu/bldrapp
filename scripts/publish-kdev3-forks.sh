#!/usr/bin/env bash
# Create danryu/*/kdev3 branches: minimal clean diffs from mojyack/main per audit.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)/kdev3-fork-work"
UPSTREAM="${UPSTREAM:-mojyack}"
OWNER="${GITHUB_OWNER:-danryu}"
BRANCH=kdev3

mkdir -p "$ROOT"
cd "$ROOT"

clone_main() {
  local name=$1
  rm -rf "$name"
  git clone -b main "https://github.com/${UPSTREAM}/${name}.git" "$name"
}

push_branch() {
  local name=$1
  cd "$name"
  git push -f "https://github.com/${OWNER}/${name}.git" "$BRANCH"
  cd ..
}

write_gstjitsimeet_submodules() {
  cat > submodules.txt <<EOF
libjitsimeet https://github.com/${OWNER}/libjitsimeet.git ${BRANCH}
cutil https://github.com/${UPSTREAM}/cutil.git 727f2eb4fced457d3b8ccf21e82408a3a4fa3df5
cutil-macros https://github.com/${UPSTREAM}/cutil-macros.git 986ced60b3c5c99364201f83a26ee5433a91a5f8
gstreamer-utils https://github.com/${UPSTREAM}/gstreamer-utils.git fd7ecf85a4031a74f5cda92bce6feac809116a45
EOF
}

write_libjitsimeet_submodules() {
  cat > submodules.txt <<EOF
crypto-utils https://github.com/${UPSTREAM}/crypto-utils.git 2c6e3b8ccbc6cdd29680f90580066dc8bc784adc
cutil https://github.com/${UPSTREAM}/cutil.git 727f2eb4fced457d3b8ccf21e82408a3a4fa3df5
cutil-macros https://github.com/${UPSTREAM}/cutil-macros.git 986ced60b3c5c99364201f83a26ee5433a91a5f8
serde https://github.com/${UPSTREAM}/serde.git 7382010975b9aff36ff672e4b9e201bb0c279e84
tinyjson https://github.com/${UPSTREAM}/tinyjson.git 018d47cd844e1a339c213f66494b0fce1f18e31f
tinyxml https://github.com/${UPSTREAM}/tinyxml.git e358c62a9024e040f14e8d0086718f5311a8ce89
websocket-cpp https://github.com/${OWNER}/websocket-cpp.git ${BRANCH}
EOF
}

strip_colibri_log_info() {
  python3 - <<'PY'
from pathlib import Path
path = Path("src/colibri.cpp")
text = path.read_text(encoding="utf-8")
text = text.replace('    LOG_INFO(logger, "Colibri send: {}", payload);\n', "")
path.write_text(text, encoding="utf-8")
print("stripped Colibri LOG_INFO noise from colibri.cpp")
PY
}

echo "=== websocket-cpp ==="
clone_main websocket-cpp
cd websocket-cpp
git checkout -B "$BRANCH"
python3 - <<'PY'
from pathlib import Path
path = Path("src/client.cpp")
text = path.read_text(encoding="utf-8")
old_err = """    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LOG_WARN(logger, "connection error");
        ctx->state = State::Destroyed;"""
new_err = """    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LOG_WARN(logger, "connection error");
        ctx->wsi   = nullptr;
        ctx->state = State::Destroyed;"""
old_closed = """    case LWS_CALLBACK_CLIENT_CLOSED:
        LOG_DEBUG(logger, "connection closed");
        ctx->state = State::Destroyed;"""
new_closed = """    case LWS_CALLBACK_CLIENT_CLOSED:
        LOG_DEBUG(logger, "connection closed");
        ctx->wsi   = nullptr;
        ctx->state = State::Destroyed;"""
old_send = "auto Context::send(PrependableBuffer buffer, bool text) -> bool {"
new_send = """auto Context::send(PrependableBuffer buffer, bool text) -> bool {
    if(state != State::Connected || wsi == nullptr) {
        LOG_WARN(logger, "send called on disconnected websocket; dropping {} bytes", buffer.size());
        return false;
    }"""
for o, n, label in ((old_err, new_err, "disconnect wsi null"),
                    (old_closed, new_closed, "closed wsi null"),
                    (old_send, new_send, "send guard")):
    if o not in text:
        raise SystemExit(f"missing {label}")
    text = text.replace(o, n, 1)
path.write_text(text, encoding="utf-8")
print("patched client.cpp")
PY
git add src/client.cpp
git commit -m "Guard send() and null wsi on disconnect (use-after-free fix)"
cd ..
push_branch websocket-cpp

echo "=== coop ==="
clone_main coop
cd coop
git remote add fork "https://github.com/${OWNER}/coop.git"
git fetch fork kdev2
git checkout -B "$BRANCH"
git cherry-pick b0030af
cd ..
push_branch coop

echo "=== submod ==="
clone_main submod
cd submod
git remote add fork "https://github.com/${OWNER}/submod.git"
git fetch fork kdev2
git checkout -B "$BRANCH"
git checkout fork/kdev2 -- submod
git add submod
git commit -m "Portable submodule clone: fix_symlinks, Windows + macOS"
cd ..
push_branch submod

echo "=== libjitsimeet ==="
clone_main libjitsimeet
cd libjitsimeet
git remote add fork "https://github.com/${OWNER}/libjitsimeet.git"
git fetch fork kdev2
git checkout -B "$BRANCH"
git cherry-pick 812bdd6
git cherry-pick 193bb84
git cherry-pick b098267
git cherry-pick 0345c46
write_libjitsimeet_submodules
strip_colibri_log_info
git add submodules.txt src/colibri.cpp
git commit -m "kdev3: upstream submodule pins; websocket-cpp fork; drop Colibri debug logs"
cd ..
push_branch libjitsimeet

echo "=== gstjitsimeet ==="
clone_main gstjitsimeet
cd gstjitsimeet
git remote add fork "https://github.com/${OWNER}/gstjitsimeet.git"
git fetch fork kdev2
git checkout -B "$BRANCH"
# Take clean feature + static sources from kdev2; drop build script/docs/example churn.
git checkout fork/kdev2 -- \
  src/props.hpp src/props.cpp src/jitsibin.hpp src/jitsibin.cpp src/lib.cpp meson.build
rm -f src/macros src/util
write_gstjitsimeet_submodules
git add -A
git commit -m "$(cat <<'EOF'
kdev3: mute/resolution props, Colibri API, static plugin build.

- server-port, audio/video-muted, receive-max-height GObject properties
- Colibri constraints after Jingle accept; per-source max-height API
- PrependableBuffer websocket handlers; jitsi/ include prefix (no duplicate symlinks)
- GST_PLUGIN_DEFINE + static-friendly meson (examples commented out)
- submodules.txt: upstream 4-entry layout, libjitsimeet -> danryu fork kdev3
EOF
)"
cd ..
push_branch gstjitsimeet

echo "All ${BRANCH} branches pushed to ${OWNER}/*"

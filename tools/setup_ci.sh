#!/bin/bash
# Source in a Linux x86-64 build job. Downloads are cached between runs.
set -euo pipefail
FM1_CACHE="${FM1_CACHE:-$HOME/.cache/fm1}"
export JIELI_TOOLCHAIN="$FM1_CACHE/jieli/toolchain"
export AC79_SDK="$FM1_CACHE/AC79_SDK"
mkdir -p "$FM1_CACHE"
if [ ! -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ]; then
    sh base/tools/get_toolchain.sh "$FM1_CACHE/jieli"
fi
if [ ! -d "$AC79_SDK/.git" ]; then
    for attempt in 1 2 3; do
        if git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
            https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$AC79_SDK"; then
            break
        fi
        sleep 5
    done
fi
test "$(git -C "$AC79_SDK" rev-parse HEAD)" = d179b4484759423312073f5fbb232501aa491047
if [ ! -d "$FM1_CACHE/emsdk/.git" ]; then
    git clone -q https://github.com/emscripten-core/emsdk.git "$FM1_CACHE/emsdk"
fi
git -C "$FM1_CACHE/emsdk" checkout -q a2b92777574c2feda07994cd4f1079a3dfc151f8
"$FM1_CACHE/emsdk/emsdk" install 6.0.10
"$FM1_CACHE/emsdk/emsdk" activate 6.0.10
set +u
source "$FM1_CACHE/emsdk/emsdk_env.sh"
set -u
emcc --version

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Yehnah as an .rfm1: the firmware for RemoteFM1 (https://github.com/Quixotic7/RemoteFM1Android, the iOS app too).
# An .rfm1 is the firmware's host build (web/emu/yehnah_web.c) compiled to WebAssembly by FM1VST's glue
# (https://github.com/Quixotic7/FM1VST, docs/RFM1.md; the format: RemoteFM1IOS docs/RFM1-FORMAT.md). The app runs it
# on the phone, with the FM-1 (running RemoteFM1's own firmware) or the on-screen panel as the controls.
#   tools/rfm1/build_rfm1.sh   ->  build/rfm1/yehnah-<version>.rfm1 (checked by rfm1_check.py, booted by rfm1_run)
# Needs: Emscripten 6.0.13 (emcc on PATH, or EMSDK=~/emsdk), CMake >= 3.22 (CMAKE=...), Python 3 with Pillow and
# fontTools (the generate step: tools/build.py generate(), into build/gen as ./build.sh does).
# FM1VST is checked out at FM1VST_COMMIT in $FM1VST (default ~/fm1vst, outside this tree); this folder's glue is
# copied into its core-glue/yehnah_web/ and this tree linked as its cores/YehNah.
set -e
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
FM1VST=${FM1VST:-$HOME/fm1vst}
FM1VST_COMMIT=ea5e6043a3dedc92cc1079ab4c2ce4c5494b583a   # the commit that built the catalog's yehnah-10.26.rfm1
EMSDK=${EMSDK:-$HOME/emsdk}
command -v emcc >/dev/null 2>&1 || PATH="$EMSDK/upstream/emscripten:$PATH"
command -v emcc >/dev/null 2>&1 || { echo "rfm1: no emcc (Emscripten 6.0.13; EMSDK=...)"; exit 1; }
CMAKE=${CMAKE:-$(command -v cmake || echo "$HOME/tools/cmake-3.31.6-linux-x86_64/bin/cmake")}
[ -x "$CMAKE" ] || { echo "rfm1: no cmake (CMAKE=...)"; exit 1; }

if [ ! -d "$FM1VST/.git" ]; then
    git init -q "$FM1VST"
    git -C "$FM1VST" remote add origin https://github.com/Quixotic7/FM1VST.git
fi
if [ "$(git -C "$FM1VST" rev-parse HEAD 2>/dev/null)" != "$FM1VST_COMMIT" ]; then
    git -C "$FM1VST" fetch -q --depth 1 origin "$FM1VST_COMMIT"
    git -C "$FM1VST" checkout -q "$FM1VST_COMMIT"
fi
cp tools/rfm1/core_yehnah.c tools/rfm1/core_yehnah_desc.c tools/rfm1/core_yehnah.cmake "$FM1VST/core-glue/yehnah_web/"
ln -sfn "$ROOT" "$FM1VST/cores/YehNah"

B="$ROOT/build/rfm1-cmake"
"$CMAKE" -S "$FM1VST" -B "$B" -DFM1_RFM1_ONLY=ON -DFM1_PUBLIC_BUILD=ON -DFM1_ONLY_CORES=yehnah \
    -DCR_PYTHON="$(command -v python3)" >/dev/null
"$CMAKE" --build "$B" --target rfm1_yehnah rfm1_run -j"$(nproc)"
mkdir -p build/rfm1
cp "$B"/rfm1/yehnah-*.rfm1 build/rfm1/
F=$(ls -t build/rfm1/yehnah-*.rfm1 | head -1)
python3 "$FM1VST/tools/rfm1_check.py" "$F"
"$B/rfm1_run" "$F" ${RFM1_RUN_ARGS:-}
echo "rfm1: $F ($(wc -c < "$F" | tr -d ' ') B)"

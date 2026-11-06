#!/usr/bin/env bash
# Compiles the C++ game (api.cpp + contexto.cpp) to WebAssembly for the website.
# Output: ../wasm/contexto.js - one self-contained file holding the compiled C++ AND the
# word vectors, so index.html works on GitHub Pages and even when opened straight from disk.
#
# Needs Emscripten (https://emscripten.org): run from an "emsdk_env" shell.
# GitHub Actions runs this automatically on every push (see .github/workflows/pages.yml).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p ../wasm

# em++ (not emcc) so the C++ standard library is linked in
em++ -O2 -std=c++14 \
    wasm_api.cpp api.cpp contexto.cpp \
    --embed-file data/vectors.bin@/vectors.bin \
    -sMODULARIZE=1 -sEXPORT_NAME=createContexto \
    -sEXPORTED_FUNCTIONS=_contexto_init,_contexto_call \
    -sEXPORTED_RUNTIME_METHODS=ccall \
    -sALLOW_MEMORY_GROWTH=1 \
    -sSINGLE_FILE=1 \
    -sENVIRONMENT=web,worker \
    -o ../wasm/contexto.js

echo "built ../wasm/contexto.js ($(wc -c < ../wasm/contexto.js) bytes)"

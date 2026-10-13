#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
# Build do host (laboratório). Falha com mensagem clara se faltar pré-requisito.
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/host}"
for c in cmake ninja g++ openssl python3; do
  command -v "$c" >/dev/null 2>&1 || { echo "BLOQUEADO: '$c' ausente no PATH"; exit 3; }
done
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLAB_TEST_MODE=ON -DBUILD_TESTING=ON
cmake --build "$BUILD_DIR" --parallel
echo "BUILD OK: $BUILD_DIR/nexusvote-cli"

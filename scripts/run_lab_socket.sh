#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/host}"
CLI="$BUILD_DIR/nexusvote-cli"
W="/tmp/nvlab-sock"; rm -rf "$W"; mkdir -p "$W"
"$CLI" --lab "$W" lab-serve & SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT
for i in $(seq 1 50); do [ -S "$W/lab.sock" ] && break; sleep 0.1; done
[ -S "$W/lab.sock" ] || { echo "FALHA: socket não criado"; exit 1; }
python3 tools/lab_client.py "$W/lab.sock" config/demo-election.json RUN-SOCK-1
ls "$W/runs/RUN-SOCK-1/out"
python3 tools/tally_verifier_python/verify.py "$W/runs/RUN-SOCK-1/out"
kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
echo "LAB-SOCKET OK"

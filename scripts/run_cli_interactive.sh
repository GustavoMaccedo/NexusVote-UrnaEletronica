#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/host}"
CLI="$BUILD_DIR/nexusvote-cli"
CFG="$PWD/config/demo-election.json"
export NEXUSVOTE_CONFIG="$CFG"
W="/tmp/nvcli-inter"; rm -rf "$W"; mkdir -p "$W"
"$CLI" --lab "$W" init "$CFG" >/dev/null
"$CLI" --lab "$W" open >/dev/null
printf '1\nC\n11111\n\nB\n\n' | "$CLI" --lab "$W" vote > "$W/inter.log" 2>&1
grep -q "Voto registrado" "$W/inter.log" || { echo "FALHA: sem confirmação"; cat "$W/inter.log"; exit 1; }
grep -q "11111" "$W/inter.log" && [ "$(grep -c 'Candidato: Ada' "$W/inter.log")" -gt 1 ] && {
  echo "FALHA: escolha anterior vaza após confirmação"; exit 1; }
"$CLI" --lab "$W" export "$W/out" >/dev/null
python3 tools/tally_verifier_python/verify.py "$W/out"
echo "CLI-INTERACTIVE OK"

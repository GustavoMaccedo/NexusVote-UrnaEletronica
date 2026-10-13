#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/host}"
CLI="$BUILD_DIR/nexusvote-cli"
CFG="$PWD/config/demo-election.json"
SEEDS="${*:-$(cat tests/fixtures/seeds.txt)}"
pass=0; fail=0
for seed in $SEEDS; do
  for n in 5 50; do
    W="/tmp/nvprop/${seed}_${n}"; rm -rf "$W"; mkdir -p "$W/work" "$W/out"
    python3 tools/fixture_generator/gen.py "$CFG" "$seed" "$n" "$W/fix.txt" >/dev/null
    export NEXUSVOTE_CONFIG="$CFG"
    "$CLI" --lab "$W/work" init "$CFG" >/dev/null
    "$CLI" --lab "$W/work" open >/dev/null
    "$CLI" --lab "$W/work" vote-batch "$W/fix.txt" >/dev/null
    "$CLI" --lab "$W/work" close >/dev/null
    "$CLI" --lab "$W/work" export "$W/out" >/dev/null
    if python3 tools/tally_verifier_python/verify.py "$W/out" >/dev/null; then
      echo "PASS seed=$seed n=$n"; pass=$((pass+1))
    else
      echo "FAIL seed=$seed n=$n (fixture e out preservados em $W)"; fail=$((fail+1))
    fi
  done
done
echo "property: $pass PASS, $fail FAIL"
[ "$fail" -eq 0 ]

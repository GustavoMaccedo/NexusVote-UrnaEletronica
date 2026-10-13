#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/host}"
if command -v cargo >/dev/null 2>&1; then
  echo "== Rust =="
  export PATH="$HOME/.cargo/bin:$PATH"
  export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-/tmp/nvbuild/rust-target}"
  cargo test -p nv-secure-core -p nv-hal --lib --manifest-path rust/Cargo.toml -q
else
  echo "== Rust: cargo ausente, pulando (BLOQUEADO) =="
fi
echo "== C++ =="
ctest --test-dir "$BUILD_DIR" --output-on-failure
echo "== Python (verificador) =="
python3 -m pytest -p no:cacheprovider tools/tally_verifier_python -q
echo "== Python (cerimônia) =="
python3 -m pytest -p no:cacheprovider tools/ceremony -q

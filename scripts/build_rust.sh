#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$HOME/.cargo/bin:$PATH"
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-/tmp/nvbuild/rust-target}"
command -v cargo >/dev/null || { echo "BLOQUEADO: cargo ausente (instale rustup)"; exit 3; }
echo "== cargo check =="
cargo check -p nv-secure-core -p nv-hal -p nv-secure-ffi -p nv-power-daemon -p nv-input-bridge --manifest-path rust/Cargo.toml
echo "== cargo test =="
cargo test -p nv-secure-core -p nv-hal --lib --manifest-path rust/Cargo.toml
echo "== bins Pi (power-daemon + input-bridge) =="
cargo build -p nv-power-daemon -p nv-input-bridge --manifest-path rust/Cargo.toml
echo OK rust

#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-/tmp/nvbuild/host}"
RUST_BIN="${CARGO_TARGET_DIR:-/tmp/nvbuild/rust-target}/debug"
OUT="artifacts/nexusvote-lab-1.2.0-pi.zip"
[ -f "$BUILD_DIR/build-info.json" ] || { echo "BLOQUEADO: build-info.json ausente (rode build_host.sh)"; exit 3; }
[ -x "$BUILD_DIR/urna-pi" ] || { echo "BLOQUEADO: urna-pi ausente (rode build_host.sh)"; exit 3; }
cat > artifacts/sbom.json <<EOF
{
  "spdxVersion": "SPDX-2.3",
  "name": "nexusvote-lab-1.2.0-pi",
  "packages": [
    {"name": "gcc", "version": "$(g++ --version | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)", "license": "GPL-3.0-or-later (toolchain)"},
    {"name": "cmake", "version": "$(cmake --version | head -1 | grep -oE '[0-9.]+' | head -1)", "license": "BSD-3-Clause"},
    {"name": "openssl", "version": "$(openssl version 2>/dev/null | grep -oE '[0-9.]+[a-z]?' | head -1)", "license": "Apache-2.0"},
    {"name": "qt", "version": "6 (eglfs kiosk, opcional)", "license": "LGPL-3.0-only"},
    {"name": "rust", "version": "$(rustc --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)", "license": "MIT OR Apache-2.0 (toolchain)"},
    {"name": "ed25519-dalek", "version": "3.0 (Cargo.lock)", "license": "BSD-3-Clause"},
    {"name": "alsa-utils", "version": "aplay (som HDMI)", "license": "GPL-2.0-or-later"}
  ]
}
EOF
rm -f "$OUT"
zip -q -r "$OUT" artifacts/sbom.json artifacts/README.txt \
  overlays/nv-keypad-4x4.dts assets/sounds/confirma.wav assets/sounds/fim.wav \
  systemd/ scripts/install-pi.sh scripts/install-keypad-overlay.sh scripts/test_hw_real.sh \
  docs/hardware_fiacao.md docs/hardware_nv.md
zip -q -j "$OUT" "$BUILD_DIR/build-info.json"
[ -x "$BUILD_DIR/urna-pi" ] && zip -q -j "$OUT" "$BUILD_DIR/urna-pi"
[ -x "$BUILD_DIR/nexusvote-cli" ] && zip -q -j "$OUT" "$BUILD_DIR/nexusvote-cli"
[ -x "$RUST_BIN/nv-input-bridge" ] && zip -q -j "$OUT" "$RUST_BIN/nv-input-bridge" || true
[ -x "$RUST_BIN/nv-power-daemon" ] && zip -q -j "$OUT" "$RUST_BIN/nv-power-daemon" || true
sha256sum "$OUT" | tee "$OUT.sha256"
echo "PACKAGE OK: $OUT"

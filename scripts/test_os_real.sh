#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail

PASS=0; WARN=0; FAIL=0
say() { printf '%s\n' "$*"; }
ok() { PASS=$((PASS+1)); say "PASS $*"; }
warn() { WARN=$((WARN+1)); say "WARN $*"; }
fail() { FAIL=$((FAIL+1)); say "FAIL $*"; }

say "== NexusVote Lab - teste de SO/hardware (1.3.0-lab) =="

ARCH="$(uname -m)"
case "$ARCH" in
  aarch64) ok "arch=$ARCH (64-bit)" ;;
  x86_64) warn "arch=$ARCH (PC de build; terminal real exige aarch64/Pi 4B)" ;;
  *) fail "arch=$ARCH (esperado x86_64 no PC ou aarch64 no terminal)" ;;
esac

if [ -f /proc/device-tree/model ]; then
  MODEL="$(tr -d '\0' < /proc/device-tree/model)"
  say "INFO model=$MODEL"
  case "$MODEL" in
    *"Raspberry Pi 4"*|*"Raspberry Pi 5"*) ok "placa suportada: $MODEL" ;;
    *) warn "placa não é Pi 4/5 (lab-host ok, terminal real exige Pi 4/5)" ;;
  esac
else
  warn "sem /proc/device-tree/model (lab-host; no Pi real existe)"
fi

if grep -q aarch64 /proc/version 2>/dev/null; then ok "kernel aarch64"; else warn "kernel sem marca aarch64"; fi

TDIR="${TMPDIR:-/tmp}/nv-os-check-$$"
mkdir -p "$TDIR"
if printf '#!/bin/sh\nexit 0\n' > "$TDIR/t.sh" && chmod +x "$TDIR/t.sh" && "$TDIR/t.sh"; then
  ok "fs exec+run em $TDIR"
else
  fail "fs sem exec em $TDIR (use ext4/tmpfs com exec)"
fi
rm -rf "$TDIR"

if python3 -c "import os; p='/tmp/nv-fsync-$$.bin'; f=os.open(p,os.O_CREAT|os.O_WRONLY,0o600); os.write(f,b'x'*4096); os.fsync(f); os.close(f); assert open(p,'rb').read()==b'x'*4096; os.unlink(p)" 2>/dev/null; then
  ok "fsync funcional"
else
  fail "fsync quebrou"
fi

if [ -f /run/systemd/timesync/synchronized ] || [ -d /var/lib/chrony ] || command -v chronyd >/dev/null 2>&1; then
  ok "relógio sincronizado (timesync/chrony)"
else
  warn "relógio sem prova de sync (no terminal real exigir chrony + RTC Pi5)"
fi

if [ -f /sys/class/rtc/rtc0/since_epoch ]; then
  EPOCH="$(cut -d' ' -f1 /sys/class/rtc/rtc0/since_epoch 2>/dev/null || echo 0)"
  if [ "${EPOCH:-0}" -ge 1704067200 ] 2>/dev/null; then ok "rtc0 são (epoch=$EPOCH)"; else warn "rtc0 zerado/maluco (epoch=$EPOCH; trocar bateria do RTC)"; fi
else
  warn "sem rtc0 (lab-host; no terminal exigir DS3231; tempo segue não-autoritativo)"
fi

if ls /sys/class/power_supply/*/online >/dev/null 2>&1 || ls /sys/class/power_supply/*/capacity >/dev/null 2>&1; then
  say "INFO power_supply: $(grep -H . /sys/class/power_supply/*/online /sys/class/power_supply/*/capacity 2>/dev/null | tr '\n' ' ')"
  ok "sensor de energia presente"
else
  warn "sem sensor de energia (lab-host; no terminal exigir UPS HAT)"
fi

if grep -q verity /proc/cmdline 2>/dev/null; then ok "dm-verity ativo"; else warn "sem dm-verity (esperado no lab; exigir no piloto selado)"; fi

if [ -e /dev/gpiochip0 ]; then ok "gpiochip0 presente"; else warn "sem /dev/gpiochip0 (ok no lab-host; no Pi real deve existir)"; fi

if [ -e /dev/tpm0 ] || [ -e /dev/tpmrm0 ]; then ok "TPM presente"; else warn "sem TPM (modo DEGRADED documentado; usar ATECC608 ou swtpm no piloto)"; fi

say "INFO fonte exigida: Pi4=5V/3A, Pi5=5V/5A (verificar etiqueta da fonte + cabo USB-C 5A p/ Pi5)"

if command -v cargo >/dev/null 2>&1 && [ -f rust/Cargo.toml ]; then
  export PATH="$HOME/.cargo/bin:$PATH"
  export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-/tmp/nvbuild/rust-target}"
  if cargo build -q -p nv-power-daemon --manifest-path rust/Cargo.toml 2>/dev/null; then
    DWORK="$(mktemp -d /tmp/nv-power-XXXXXX)"
    BIN="$CARGO_TARGET_DIR/debug/nv-power-daemon"
    if NV_HAL_STOP=1 NV_HAL_POLL_MS=50 "$BIN" "$DWORK" 2>/dev/null && grep -q '"evento":"boot"' "$DWORK/hw-events.jsonl" 2>/dev/null; then
      ok "power_daemon: boot no bordo"
    else
      warn "power_daemon não registrou boot"
    fi
    rm -rf "$DWORK"
  else
    warn "power_daemon não compilou (verificar)"
  fi
else
  warn "cargo ausente; pulo power_daemon"
fi

say "== resultado: $PASS PASS, $WARN WARN, $FAIL FAIL =="
[ "$FAIL" -eq 0 ]

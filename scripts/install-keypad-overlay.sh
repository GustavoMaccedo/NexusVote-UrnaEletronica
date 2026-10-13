#!/bin/bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DTS="$HERE/overlays/nv-keypad-4x4.dts"
DTBO="nv-keypad-4x4.dtbo"
OVERLAY_DIR=""
for d in /boot/firmware/overlays /boot/overlays; do
  if [ -d "$d" ]; then OVERLAY_DIR="$d"; break; fi
done
CFG=""
for c in /boot/firmware/config.txt /boot/config.txt; do
  if [ -f "$c" ]; then CFG="$c"; break; fi
done
if [ -z "$OVERLAY_DIR" ] || [ -z "$CFG" ]; then
  echo "ERRO: Pi OS não detectado (sem /boot/firmware). Rode isto NO PI." >&2
  exit 1
fi
command -v dtc >/dev/null || { echo "instalando device-tree-compiler..."; apt-get update && apt-get install -y device-tree-compiler; }
echo "[1/4] compilando $DTS"
dtc -@ -I dts -O dtb -o "/tmp/$DTBO" "$DTS"
echo "[2/4] instalando em $OVERLAY_DIR/$DTBO"
cp "/tmp/$DTBO" "$OVERLAY_DIR/$DTBO"
echo "[3/4] ativando em $CFG"
grep -q "^dtoverlay=nv-keypad-4x4" "$CFG" || echo "dtoverlay=nv-keypad-4x4" >> "$CFG"
echo "[4/4] udev symlink /dev/input/nv-keypad"
cp "$HERE/systemd/99-nv-keypad.rules" /etc/udev/rules.d/99-nv-keypad.rules
udevadm control --reload-rules || true
modprobe matrix-keypad || true
echo "OK. Reinicie (sudo reboot) e valide com: evtest /dev/input/nv-keypad"

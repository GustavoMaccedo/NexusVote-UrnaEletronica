#!/bin/bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -uo pipefail
PASS=0; FAIL=0
ok() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; FAIL=$((FAIL+1)); }

echo "== NV HIL $(date -u +%FT%TZ) =="
MODEL="$(tr -d '\0' < /proc/device-tree/model 2>/dev/null || echo "?")"
echo "placa: $MODEL / $(uname -m)"
case "$MODEL" in *Raspberry*Pi*4*|*Raspberry*Pi*5*) ok "placa suportada ($MODEL)";; *) fail "placa não suportada ($MODEL) - exige Pi 4B/5";; esac
if [ -e /dev/input/nv-keypad ]; then ok "teclado /dev/input/nv-keypad presente"; else fail "teclado ausente (dtoverlay=nv-keypad-4x4? reboot? dmesg|grep matrix)"; fi
if command -v evtest >/dev/null; then
  timeout 3 evtest --query /dev/input/nv-keypad EV_KEY KEY_1 2>/dev/null && ok "evdev responde" || echo "INFO: evtest query pulado (pressione teclas p/ validar manual)"
else fail "evtest ausente (apt install evtest)"; fi
if [ -e /dev/dri/card0 ] || [ -e /dev/fb0 ]; then ok "display DRM/FB presente"; else fail "display ausente (/dev/dri/card0 nem /dev/fb0)"; fi
if command -v aplay >/dev/null; then
  aplay -l 2>/dev/null | grep -qi "hdmi\|vc4" && ok "ALSA HDMI detectado" || fail "ALSA sem HDMI (dtparam=audio=on? reboot?)"
  [ -f assets/sounds/confirma.wav ] && ok "confirma.wav presente" || fail "assets/sounds/confirma.wav ausente"
  [ -f assets/sounds/fim.wav ] && ok "fim.wav presente" || fail "assets/sounds/fim.wav ausente"
else fail "aplay ausente (apt install alsa-utils)"; fi
if [ -d /sys/class/pwm/pwmchip0 ]; then ok "PWM piezo presente"; else echo "INFO: PWM ausente (dtoverlay=pwm,pin=18,func=2) - HDMI cobre som"; fi
if [ -e /sys/class/rtc/rtc0/since_epoch ]; then
  EPOCH="$(cat /sys/class/rtc/rtc0/since_epoch | tr -dc 0-9)"
  if [ "${EPOCH:-0}" -ge 1704067200 ]; then ok "RTC rtc0 são (epoch $EPOCH)"; else fail "RTC zerado/maluco (epoch $EPOCH, hwclock -w?)"; fi
else fail "rtc0 ausente (dtoverlay=i2c-rtc,ds3231?)"; fi
if ls /sys/class/power_supply/* >/dev/null 2>&1; then ok "power_supply presente"; else echo "INFO: sem power_supply (UPS HAT I2C?) - power-daemon cobre via bordo"; fi
if [ -e /dev/usb/lp0 ]; then ok "impressora /dev/usb/lp0 presente"; else echo "INFO: impressora ausente - segue QR na tela + ata (IMPRESSORA-AUSENTE)"; fi
for b in /opt/nexusvote-lab/urna-pi /opt/nexusvote-lab/nexusvote-cli; do
  [ -x "$b" ] && ok "binário $b" || fail "binário ausente $b (rode install-pi.sh)"
done
echo "== resultado: $PASS pass, $FAIL fail =="
[ "$FAIL" -eq 0 ]

#!/bin/bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
ROLE="${1:-te}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
echo "== NexusVote Lab install-pi ($ROLE) =="
if [ -z "${NV_ALLOW_NON_PI:-}" ] && ! tr -d '\0' < /proc/device-tree/model 2>/dev/null | grep -q "Raspberry Pi"; then
  echo "BLOQUEADO: este script é exclusivo do Raspberry Pi (use NV_ALLOW_NON_PI=1 para forçar)"
  exit 3
fi
apt-get update
apt-get install -y alsa-utils evtest i2c-tools device-tree-compiler \
  qt6-base-dev libasound2-dev libdrm-dev libgbm-dev libinput-dev \
  build-essential cmake libssl-dev pkg-config curl
if ! command -v cargo >/dev/null; then
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
fi
export PATH="$HOME/.cargo/bin:$PATH"
id urna >/dev/null 2>&1 || useradd -m -G audio,gpio,lp,input,dialout urna || true
usermod -aG audio,gpio,lp,input,dialout "${SUDO_USER:-pi}" || true
bash "$HERE/scripts/install-keypad-overlay.sh" || true
cp "$HERE/systemd/99-nv-keypad.rules" /etc/udev/rules.d/ || true
udevadm control --reload-rules || true
CFG=/boot/firmware/config.txt; [ -f "$CFG" ] || CFG=/boot/config.txt
for line in "dtparam=i2c_arm=on" "dtoverlay=i2c-rtc,ds3231" "dtoverlay=pwm,pin=18,func=2" \
  "hdmi_force_hotplug=1" "hdmi_group=2" "hdmi_mode=87" "hdmi_cvt 800 480 60 6 0 0 0" \
  "hdmi_drive=2" "dtoverlay=vc4-kms-v3d" "max_framebuffers=2" "dtparam=audio=on"; do
  grep -qF "$line" "$CFG" || echo "$line" >> "$CFG"
done
apt-get purge -y fake-hwclock || true
timedatectl set-ntp false || true
mkdir -p /opt/nexusvote-lab /var/lib/nexusvote-lab/work
export CARGO_TARGET_DIR=/tmp/nvbuild/rust-target BUILD_DIR=/tmp/nvbuild/host
bash "$HERE/scripts/build_host.sh"
bash "$HERE/scripts/build_rust.sh"
install -m 0755 /tmp/nvbuild/host/nexusvote-cli /opt/nexusvote-lab/
install -m 0755 /tmp/nvbuild/host/urna-pi /opt/nexusvote-lab/
install -m 0755 /tmp/nvbuild/rust-target/debug/nv-input-bridge /opt/nexusvote-lab/
install -m 0755 /tmp/nvbuild/rust-target/debug/nv-power-daemon /opt/nexusvote-lab/
cp -r "$HERE/assets" /opt/nexusvote-lab/ || true
cp -r "$HERE/config" /opt/nexusvote-lab/ || true
cp "$HERE/systemd/"*.service /etc/systemd/system/ || true
systemctl daemon-reload || true
echo "OK. Reinicie (sudo reboot). Depois: sudo bash scripts/test_hw_real.sh"
echo "Kiosk Qt: sudo systemctl enable --now nexusvote-qt  | Terminal texto: sudo systemctl enable --now nexusvote-terminal"

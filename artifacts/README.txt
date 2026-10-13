Pacote do terminal Pi (13/10/2026).

- nexusvote-lab-1.2.0-pi.zip (+ .sha256): urna-pi, nexusvote-cli,
  nv-input-bridge, nv-power-daemon, build-info.json, sbom.json,
  overlay DTS 4x4, sons WAV, systemd e scripts
  (install-pi/install-keypad-overlay/test_hw_real).
  Gerado por scripts/package_artifacts.sh.

- sbom.json: inventário mínimo de dependências (SPDX-lite).

No Pi (Pi OS Lite 64-bit, Pi 4B):
  sudo bash scripts/install-pi.sh && sudo reboot &&
  sudo bash scripts/test_hw_real.sh   # tudo PASS = pronto p/ votar

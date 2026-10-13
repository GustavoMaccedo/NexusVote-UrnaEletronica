#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
# Trava a versão única do protótipo (núcleo C++ == terminal físico == docs-chave).
set -euo pipefail
cd "$(dirname "$0")/.."
WANT="${1:-1.2.0-lab}"
fail=0
check() { grep -qF "$2" "$1" || { echo "VERSAO: $1 sem '$2'"; fail=1; }; }
check CMakeLists.txt "project(nexusvote VERSION 1.2.0"
check CMakeLists.txt 'NEXUSVOTE_VERSION="1.2.0-lab"'
check CMakeLists.txt 'urna-pi'
if [ -e android-test-app ]; then echo "VERSAO: android-test-app ressurgiu (diretório extinto)"; fail=1; fi
[ "$fail" -eq 0 ] && echo "VERSION OK ($WANT)" || exit 1

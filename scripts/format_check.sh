#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
command -v clang-format >/dev/null 2>&1 || { echo "BLOQUEADO: clang-format ausente"; exit 3; }
clang-format --dry-run --Werror src/core/*.hpp src/core/*.cpp src/cli/*.cpp \
  src/secure/*.hpp src/secure/*.cpp src/lab/*.hpp src/lab/*.cpp \
  src/paper/*.hpp src/paper/*.cpp src/terminal/*.cpp \
  src/ui/*.hpp src/ui/qt/*.hpp src/ui/qt/*.cpp tests/*.cpp tests/*.hpp
if grep -rIn " $" --include="*.cpp" --include="*.hpp" --include="*.py" \
    --include="*.sh" --include="*.dts" \
    src tests tools scripts overlays systemd 2>/dev/null | grep -v ".gradle"; then
  echo "FALHA: trailing whitespace acima"; exit 1
fi
if [ -d .git ]; then git diff --check; fi
echo "FORMAT OK"

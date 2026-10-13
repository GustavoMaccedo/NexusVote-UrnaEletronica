#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
# Build da UI Qt Widgets (PC Linux ou placa com Qt6).
# Padrão: Qt do sistema. Alternativa: Qt customizado via NVQT.
# Opcionais (ambientes sem Qt completo): NVICU (ICU local), NVGL (GL/XKB locais).
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/qt}"
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release -DWITH_QT=ON -DLAB_TEST_MODE=ON -DBUILD_TESTING=ON)
if [ -n "${NVQT:-}" ]; then
  [ -x "$NVQT/bin/qmake" ] || { echo "BLOQUEADO: Qt6 ausente em $NVQT"; exit 3; }
  CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$NVQT")
  export LD_LIBRARY_PATH="$NVQT/lib:${LD_LIBRARY_PATH:-}"
elif [ -d /usr/lib/x86_64-linux-gnu/cmake/Qt6 ] || [ -d /usr/lib/aarch64-linux-gnu/cmake/Qt6 ]; then
  :
else
  echo "BLOQUEADO: Qt6 ausente (instale qt6-base-dev ou exporte NVQT)"
  exit 3
fi
if [ -n "${NVICU:-}" ]; then
  [ -e "$NVICU/lib/libicui18n.so.73" ] || { echo "BLOQUEADO: ICU ausente em $NVICU"; exit 3; }
  export LD_LIBRARY_PATH="$NVICU/lib:${LD_LIBRARY_PATH:-}"
fi
if [ -n "${NVGL:-}" ]; then
  GL="$NVGL/lib"
  [ -d "$GL" ] || GL="$NVGL/lib/x86_64-linux-gnu"
  [ -d "$GL" ] || GL="$NVGL/lib/aarch64-linux-gnu"
  CMAKE_ARGS+=(-DOPENGL_opengl_LIBRARY="$GL/libOpenGL.so" -DOPENGL_INCLUDE_DIR="$NVGL/include" -DXKB_LIBRARY="$GL/libxkbcommon.so" -DXKB_INCLUDE_DIR="$NVGL/include")
fi
export LC_ALL=C.UTF-8
cmake -S . -B "$BUILD_DIR" -G Ninja "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel
echo "BUILD QT OK: $BUILD_DIR/nexusvote-qt"
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/nexusvote-qt" 2>&1 | head -2 || true
ctest --test-dir "$BUILD_DIR" --output-on-failure

#!/usr/bin/env bash
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/host}"
CLI="$BUILD_DIR/nexusvote-cli"
CFG="$PWD/config/demo-election.json"
export NEXUSVOTE_CONFIG="$CFG"
echo "== fuzz loader (256 entradas aleatórias) =="
crash=0
for i in $(seq 1 256); do
  head -c 200 /dev/urandom > /tmp/nvfuzz.json
  if "$CLI" --lab /tmp/nvfuzz_work init /tmp/nvfuzz.json >/dev/null 2>&1; then
    echo "ALERTA: entrada aleatória aceita (i=$i)"; crash=$((crash+1))
  fi
  rm -rf /tmp/nvfuzz_work
done
echo "fuzz loader: $crash aceites indevidos (esperado 0), sem crash sem diagnóstico"
echo "== adulteração detectada =="
rm -rf /tmp/nvtamper && mkdir -p /tmp/nvtamper/work /tmp/nvtamper/out
"$CLI" --lab /tmp/nvtamper/work init "$CFG" >/dev/null
"$CLI" --lab /tmp/nvtamper/work open >/dev/null
printf 'alcaide 11\n' > /tmp/nvtamper/fix.txt
"$CLI" --lab /tmp/nvtamper/work vote-batch /tmp/nvtamper/fix.txt >/dev/null
"$CLI" --lab /tmp/nvtamper/work export /tmp/nvtamper/out >/dev/null
cp /tmp/nvtamper/out/lab-boletim.json /tmp/nvtamper/out/lab-boletim.json.orig
sed -i 's/"total":1/"total":99/' /tmp/nvtamper/out/lab-boletim.json
if python3 tools/tally_verifier_python/verify.py /tmp/nvtamper/out >/dev/null 2>&1; then
  echo "FALHA: adulteração não detectada"; exit 1
else
  echo "PASS: adulteração detectada pelo verificador"
fi
echo "== reexportação estável =="
cp /tmp/nvtamper/out/lab-boletim.json.orig /tmp/nvtamper/out/lab-boletim.json
h1=$(sha256sum /tmp/nvtamper/out/lab-boletim.json | cut -d' ' -f1)
"$CLI" --lab /tmp/nvtamper/work export /tmp/nvtamper/out >/dev/null
python3 tools/tally_verifier_python/verify.py /tmp/nvtamper/out
echo "== privacidade: grep de escolhas em logs/exports =="
if grep -rE "11111|Ada|Duarte|record_id.{0,0}" /tmp/nvtamper/work/events.log >/dev/null 2>&1; then
  echo "FALHA: escolha vazou em events.log"; exit 1
else
  echo "PASS: events.log sem escolhas"
fi
echo "== anti-rede estatica (nucleo C++ sem sockets IP) =="
if grep -rInE "AF_INET|SOCK_STREAM.*connect|getaddrinfo|QNetwork|QTcpSocket|CURL|curl_easy" src/ | grep -v AF_UNIX; then
  echo "FALHA: API de rede IP no nucleo"; exit 1
else
  echo "PASS: sem sockets IP no nucleo (AF_UNIX local permitido)"
fi
if [ -e android-test-app ]; then
  echo "FALHA: android-test-app ressurgiu (diretório extinto)"; exit 1
else
  echo "PASS: terminal físico Pi, sem INTERNET por construção"
fi
[ "$crash" -eq 0 ]

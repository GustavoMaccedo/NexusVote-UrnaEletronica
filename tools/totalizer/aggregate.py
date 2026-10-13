#!/usr/bin/env python3
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import json
import subprocess
import sys
from pathlib import Path

VERIFY = Path(__file__).parent.parent / "tally_verifier_python" / "verify.py"
BANNER = "NÃO OFICIAL"

def fail(msg: str) -> int:
    print(f"FAIL: {msg}")
    return 1

def main() -> int:
    if len(sys.argv) < 2:
        print("uso: aggregate.py <outdir1> [<outdir2> ...]")
        return 2
    total_bus = 0
    agg: dict = {}
    for out in sys.argv[1:]:
        r = subprocess.run([sys.executable, str(VERIFY), out], capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout + r.stderr)
            return fail(f"bundle inválido: {out}")
        try:
            bol = json.load(open(f"{out}/lab-boletim.json", encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as e:
            return fail(f"boletim ilegível em {out}: {e}")
        if BANNER not in str(bol.get("banner", "")):
            return fail(f"banner ausente em {out}")
        total_bus += 1
        for office, t in bol.get("offices", {}).items():
            slot = agg.setdefault(office, {"nom": {}, "leg": {}, "blank": 0, "null": 0, "total": 0})
            for cid, n in t.get("nominais", {}).items():
                slot["nom"][cid] = slot["nom"].get(cid, 0) + n
            for leg, n in t.get("legendas", {}).items():
                slot["leg"][leg] = slot["leg"].get(leg, 0) + n
            slot["blank"] += t.get("brancos", 0)
            slot["null"] += t.get("nulos", 0)
            slot["total"] += t.get("total", 0)
    print(f"OK: {total_bus} BUs agregados; totais coincidem.")
    print(json.dumps({"schema_version": 1, "banner": BANNER + " - TOTALIZACAO LAB",
                      "bus": total_bus, "offices": agg}, indent=2, ensure_ascii=False))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

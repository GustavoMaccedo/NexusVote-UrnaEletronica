#!/usr/bin/env python3
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import json
import random
import sys

def main() -> int:
    cfg_path, seed, n, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    cfg = json.load(open(cfg_path, encoding="utf-8"))
    rng = random.Random(seed)
    offices = cfg["offices"]
    cands = [c for c in cfg["candidates"] if c.get("eligible", True)]
    lines = [f"# fixture sintética seed={seed} (fictícia, NÃO OFICIAL)"]
    for _ in range(n):
        off = rng.choice(offices)
        oid = off["office_id"]
        oc = [c for c in cands if c["office_id"] == oid]
        roll = rng.random()
        if roll < 0.55 and oc:
            lines.append(f"{oid} {rng.choice(oc)['number']}")
        elif roll < 0.65 and cfg.get("legend_enabled") and off.get("proportional"):
            parties = [p["party_number"] for p in cfg.get("parties", [])]
            pn = rng.choice(parties) if parties else "99"
            lines.append(f"{oid} {pn}{'9' * (off['number_length'] - 2)}")
        elif roll < 0.80:
            lines.append(f"{oid} BRANCO")
        elif roll < 0.92:
            lines.append(f"{oid} {'0' * off['number_length']}")
        else:
            lines.append(f"{oid} {'8' * off['number_length']}")
    open(out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print(f"fixture seed={seed}: {n} votos -> {out}")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("uso: gen.py <config.json> <seed> <n> <saida>")
        sys.exit(2)
    sys.exit(main())

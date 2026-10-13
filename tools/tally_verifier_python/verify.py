#!/usr/bin/env python3
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import ed25519

def fail(msg: str) -> int:
    print(f"FAIL: {msg}")
    return 1

def check_signature(out: str):
    try:
        sigj = json.load(open(f"{out}/lab-signature.json", encoding="utf-8"))
        pubhex = open(f"{out}/lab-pubkey.hex", encoding="utf-8").read().strip()
    except OSError:
        return None
    try:
        pub = bytes.fromhex(pubhex)
        sig = bytes.fromhex(sigj.get("sig_boletim", ""))
        msg = open(f"{out}/lab-boletim.json", "rb").read()
    except (ValueError, OSError):
        return False
    if not sig or not pub:
        return None
    return True if ed25519.verify(pub, msg, sig) else False

def main() -> int:
    if len(sys.argv) != 2:
        print("uso: verify.py <outdir>")
        return 2
    out = sys.argv[1]
    try:
        manifest = open(f"{out}/manifest.sha256", encoding="utf-8").read().splitlines()
    except OSError as e:
        return fail(f"manifesto inacessível: {e}")
    for line in manifest:
        if not line.strip():
            continue
        hexsum, name = line.split(None, 1)
        name = name.strip()
        try:
            data = open(f"{out}/{name}", "rb").read()
        except OSError:
            return fail(f"artefato ausente: {name}")
        if hashlib.sha256(data).hexdigest() != hexsum:
            return fail(f"hash divergente (adulteração?) em {name}")
    try:
        rdv = json.load(open(f"{out}/lab-rdv.json", encoding="utf-8"))
        bol = json.load(open(f"{out}/lab-boletim.json", encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        return fail(f"parse: {e}")
    if rdv.get("schema_version") != 1 or bol.get("schema_version") != 1:
        return fail("schema_version inesperado")
    if "NÃO OFICIAL" not in str(bol.get("banner", "")):
        return fail("banner NÃO OFICIAL ausente no boletim")
    try:
        cfg_text = open(f"{out}/election-config.json", "rb").read()
        if hashlib.sha256(cfg_text).hexdigest() != bol.get("config_sha256", ""):
            return fail("config do bundle divergente do boletim")
        if json.loads(cfg_text.decode("utf-8")).get("schema_version") != 1:
            return fail("schema da config do bundle inesperado")
    except OSError:
        print("AVISO: election-config.json ausente no bundle (export antigo?)")
    try:
        zer = json.load(open(f"{out}/lab-zeresima.json", encoding="utf-8"))
        if zer.get("totals") != "all_zero":
            return fail("zerésima do bundle não está zerada")
        if zer.get("lab_session_id") != bol.get("lab_session_id", ""):
            return fail("zerésima de outra sessão")
    except OSError:
        print("AVISO: lab-zeresima.json ausente no bundle (export antigo?)")
    except json.JSONDecodeError as e:
        return fail(f"zerésima inválida: {e}")
    sig_ok = check_signature(out)
    if sig_ok is False:
        return fail("assinatura demo inválida (boletim adulterado após assinatura?)")
    if sig_ok is None:
        print("AVISO: sem assinatura demo no bundle (opcional)")
    recount: dict = {}
    seen_ids = set()
    for rec in rdv.get("records", []):
        rid = rec.get("record_id", "")
        if rid in seen_ids:
            return fail("record_id duplicado")
        seen_ids.add(rid)
        off = rec.get("office_id", "")
        kind = rec.get("kind", "")
        ref = rec.get("ref", "")
        slot = recount.setdefault(off, {"nom": {}, "leg": {}, "blank": 0, "null": 0})
        if kind == "candidate":
            slot["nom"][ref] = slot["nom"].get(ref, 0) + 1
        elif kind == "party":
            slot["leg"][ref] = slot["leg"].get(ref, 0) + 1
        elif kind == "blank":
            slot["blank"] += 1
        elif kind == "null":
            slot["null"] += 1
        else:
            return fail(f"kind desconhecido: {kind}")
    offices = bol.get("offices", {})
    if set(recount.keys()) != set(offices.keys()) and recount:
        for off in recount:
            if off not in offices:
                return fail(f"cargo do RDV ausente no boletim: {off}")
    for off, b in offices.items():
        s = recount.get(off, {"nom": {}, "leg": {}, "blank": 0, "null": 0})
        if dict(s["nom"]) != dict(b.get("nominais", {})):
            return fail(f"nominais divergentes em {off}")
        if dict(s["leg"]) != dict(b.get("legendas", {})):
            return fail(f"legendas divergentes em {off}")
        if s["blank"] != b.get("brancos", -1) or s["null"] != b.get("nulos", -1):
            return fail(f"branco/nulo divergentes em {off}")
        total = sum(s["nom"].values()) + sum(s["leg"].values()) + s["blank"] + s["null"]
        if total != b.get("total", -1):
            return fail(f"total divergente em {off}: {total} != {b.get('total')}")
    print(f"OK: {len(seen_ids)} registros conferidos; totais coincidem.")
    return 0

if __name__ == "__main__":
    sys.exit(main())

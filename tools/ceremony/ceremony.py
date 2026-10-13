#!/usr/bin/env python3
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import hashlib
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "tally_verifier_python"))
import ed25519

BANNER = "PROTOTIPO INDEPENDENTE - LABORATORIO - NAO OFICIAL"

def _b58_clamp(s: bytes) -> int:
    h = bytearray(hashlib.sha512(s).digest()[:32])
    h[0] &= 248
    h[31] &= 127
    h[31] |= 64
    return int.from_bytes(bytes(h), "little")

def sign(priv: bytes, msg: bytes) -> bytes:
    h = hashlib.sha512(priv).digest()
    a = _b58_clamp(priv)
    r = int.from_bytes(hashlib.sha512(h[32:] + msg).digest(), "little")
    rPt = ed25519._scalarmult(ed25519._B, r % ed25519.L)
    R = ed25519._encodepoint(*rPt)
    A = ed25519._encodepoint(*ed25519._scalarmult(ed25519._B, a))
    S = (r + ed25519._hint(R + A + msg) * a) % ed25519.L
    return R + S.to_bytes(32, "little")

def pubkey(priv: bytes) -> bytes:
    a = _b58_clamp(priv)
    return ed25519._encodepoint(*ed25519._scalarmult(ed25519._B, a))

def cmd_gen(d: Path) -> int:
    d.mkdir(parents=True, exist_ok=True)
    privf, pubf, metaf = d / "lab-privkey.hex", d / "lab-pubkey.hex", d / "ceremony.json"
    if privf.exists() or pubf.exists():
        print("FALHA: cerimônia já existe (não sobrescrevo chave privada)")
        return 1
    priv = os.urandom(32)
    pub = pubkey(priv)
    privf.write_text(priv.hex() + "\n", encoding="utf-8")
    pubf.write_text(pub.hex() + "\n", encoding="utf-8")
    try:
        os.chmod(privf, 0o600)
    except OSError:
        pass
    meta = {
        "schema_version": 1,
        "banner": BANNER,
        "key_id": "lab-cerimonia-01",
        "alg": "Ed25519",
        "custodiantes": ["presidente-ficticio", "mesario-1-ficticio", "fiscal-ficticio"],
        "criada_em": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "pubkey": pub.hex(),
    }
    metaf.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    assert ed25519.verify(pub, b"", sign(priv, b""))
    assert ed25519.verify(pub, b"abc", sign(priv, b"abc"))
    print(f"OK cerimônia em {d} (pub {pub.hex()[:16]}...)")
    return 0

def cmd_sign(cer: Path, out: Path) -> int:
    try:
        priv = bytes.fromhex((cer / "lab-privkey.hex").read_text(encoding="utf-8").strip())
        meta = json.loads((cer / "ceremony.json").read_text(encoding="utf-8"))
    except OSError as e:
        print(f"FALHA: cerimônia inacessível: {e}")
        return 1
    if len(priv) != 32:
        print("FALHA: chave privada inválida")
        return 1
    try:
        msg = (out / "lab-boletim.json").read_bytes()
    except OSError as e:
        print(f"FALHA: boletim ausente: {e}")
        return 1
    sig = sign(priv, msg)
    pub = pubkey(priv)
    assert ed25519.verify(pub, msg, sig), "autoteste da assinatura falhou"
    (out / "lab-signature.json").write_text(
        json.dumps({
            "schema_version": 1,
            "alg": "Ed25519-ceremony",
            "key_id": meta.get("key_id", "lab-cerimonia-01"),
            "pubkey": pub.hex(),
            "sig_boletim": sig.hex(),
            "note": BANNER + ": assinatura da cerimônia (offline)",
        }, indent=2) + "\n",
        encoding="utf-8",
    )
    (out / "lab-pubkey.hex").write_text(pub.hex() + "\n", encoding="utf-8")
    lines = []
    for name in sorted(p.name for p in out.iterdir() if p.is_file() and p.name != "manifest.sha256"):
        h = hashlib.sha256((out / name).read_bytes()).hexdigest()
        lines.append(f"{h}  {name}\n")
    (out / "manifest.sha256").write_text("".join(lines), encoding="utf-8")
    print(f"OK bundle assinado em {out} (sig {sig.hex()[:16]}...)")
    return 0

def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    if cmd == "gen" and len(sys.argv) == 3:
        return cmd_gen(Path(sys.argv[2]))
    if cmd == "sign" and len(sys.argv) == 4:
        return cmd_sign(Path(sys.argv[2]), Path(sys.argv[3]))
    print(__doc__)
    return 2

if __name__ == "__main__":
    raise SystemExit(main())

# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import ed25519

VERIFY = Path(__file__).with_name("verify.py")

VEC_PUB = "0b5b643ab3199c416be8fd2084f43ef796d398ef35ae81a74722a6780fac1a77"
VEC_MSG = bytes.fromhex("626f6c6574696d2d64656d6f2d7465737465")
VEC_SIG = (
    "7175553f74663c951ccc486ed66390b3f0924184b75d79639d3b3341c956b502"
    "e60fafaaaf735f5ab7a5b0f999db0a102dd9ec80b395ca4823152604e9b69b0c"
)

def run_verify(outdir: Path) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(VERIFY), str(outdir)], capture_output=True, text=True)

def write_export(d: Path, records, boletim_offices):
    (d / "lab-rdv.json").write_text(
        json.dumps({"schema_version": 1, "lab_session_id": "LAB-T", "records": records}),
        encoding="utf-8",
    )
    (d / "lab-boletim.json").write_text(
        json.dumps(
            {
                "schema_version": 1,
                "banner": "NÃO OFICIAL — BOLETIM DE DEMONSTRAÇÃO",
                "offices": boletim_offices,
            }
        ),
        encoding="utf-8",
    )
    (d / "lab-signature.json").write_text("{}", encoding="utf-8")
    import hashlib

    lines = []
    for n in ("lab-rdv.json", "lab-boletim.json", "lab-signature.json"):
        lines.append(f"{hashlib.sha256((d / n).read_bytes()).hexdigest()}  {n}")
    (d / "manifest.sha256").write_text("\n".join(lines) + "\n", encoding="utf-8")

def test_ok():
    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        write_export(
            d,
            [
                {"record_id": "a" * 32, "office_id": "alcaide", "kind": "candidate", "ref": "c1"},
                {"record_id": "b" * 32, "office_id": "alcaide", "kind": "blank", "ref": ""},
            ],
            {"alcaide": {"nominais": {"c1": 1}, "legendas": {}, "brancos": 1, "nulos": 0, "total": 2}},
        )
        p = run_verify(d)
        assert p.returncode == 0, p.stdout + p.stderr
        assert "OK" in p.stdout

def test_tamper_detected():
    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        write_export(
            d,
            [{"record_id": "a" * 32, "office_id": "alcaide", "kind": "blank", "ref": ""}],
            {"alcaide": {"nominais": {}, "legendas": {}, "brancos": 1, "nulos": 0, "total": 1}},
        )
        (d / "lab-boletim.json").write_text(
            (d / "lab-boletim.json").read_text(encoding="utf-8").replace('"total": 1', '"total": 9'),
            encoding="utf-8",
        )
        p = run_verify(d)
        assert p.returncode == 1
        assert "FAIL" in p.stdout

def test_ed25519_vector():
    assert ed25519.verify(bytes.fromhex(VEC_PUB), VEC_MSG, bytes.fromhex(VEC_SIG))
    assert not ed25519.verify(bytes.fromhex(VEC_PUB), VEC_MSG + b"X", bytes.fromhex(VEC_SIG))
    assert not ed25519.verify(bytes(32), VEC_MSG, bytes.fromhex(VEC_SIG))

def test_signature_openssl_roundtrip():
    if shutil.which("openssl") is None:
        import pytest

        pytest.skip("openssl ausente")
    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        subprocess.run(
            ["openssl", "genpkey", "-algorithm", "ed25519", "-out", str(d / "k.pem")],
            check=True, capture_output=True,
        )
        subprocess.run(
            ["openssl", "pkey", "-in", str(d / "k.pem"), "-pubout", "-out", str(d / "p.pem")],
            check=True, capture_output=True,
        )
        der = subprocess.run(
            ["openssl", "pkey", "-in", str(d / "p.pem"), "-pubin", "-outform", "DER"],
            check=True, capture_output=True,
        ).stdout
        (d / "lab-boletim.json").write_bytes(b'{"demo":1}')
        subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-inkey", str(d / "k.pem"), "-rawin",
             "-in", str(d / "lab-boletim.json"), "-out", str(d / "sig.bin")],
            check=True, capture_output=True,
        )
        (d / "lab-pubkey.hex").write_text(der[-32:].hex() + "\n", encoding="utf-8")
        (d / "lab-signature.json").write_text(
            json.dumps({"sig_boletim": (d / "sig.bin").read_bytes().hex()}), encoding="utf-8"
        )
        sys.path.insert(0, str(Path(__file__).parent))
        from verify import check_signature

        assert check_signature(str(d)) is True
        (d / "lab-boletim.json").write_bytes(b'{"demo":2}')
        assert check_signature(str(d)) is False

def test_bundle_config_mismatch():
    import hashlib as hl

    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        write_export(
            d,
            [{"record_id": "a" * 32, "office_id": "alcaide", "kind": "blank", "ref": ""}],
            {"alcaide": {"nominais": {}, "legendas": {}, "brancos": 1, "nulos": 0, "total": 1}},
        )
        (d / "election-config.json").write_text('{"schema_version": 1}', encoding="utf-8")
        lines = (d / "manifest.sha256").read_text(encoding="utf-8").splitlines()
        lines.append(
            f"{hl.sha256((d / 'election-config.json').read_bytes()).hexdigest()}  election-config.json"
        )
        (d / "manifest.sha256").write_text("\n".join(lines) + "\n", encoding="utf-8")
        p = run_verify(d)
        assert p.returncode == 1
        assert "divergente" in p.stdout

def test_mismatch_detected():
    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        write_export(
            d,
            [{"record_id": "a" * 32, "office_id": "alcaide", "kind": "blank", "ref": ""}],
            {"alcaide": {"nominais": {}, "legendas": {}, "brancos": 0, "nulos": 0, "total": 0}},
        )
        import hashlib

        lines = []
        for n in ("lab-rdv.json", "lab-boletim.json", "lab-signature.json"):
            lines.append(f"{hashlib.sha256((d / n).read_bytes()).hexdigest()}  {n}")
        (d / "manifest.sha256").write_text("\n".join(lines) + "\n", encoding="utf-8")
        p = run_verify(d)
        assert p.returncode == 1
        assert "divergente" in p.stdout

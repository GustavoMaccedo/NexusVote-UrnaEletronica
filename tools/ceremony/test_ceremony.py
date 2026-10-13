# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "tally_verifier_python"))
import ed25519

import ceremony

def test_sign_verify_roundtrip(tmp_path):
    cer = tmp_path / "cer"
    assert ceremony.cmd_gen(cer) == 0
    priv = bytes.fromhex((cer / "lab-privkey.hex").read_text().strip())
    pub = bytes.fromhex((cer / "lab-pubkey.hex").read_text().strip())
    assert ed25519.verify(pub, b"mensagem-lab", ceremony.sign(priv, b"mensagem-lab"))
    assert ed25519.verify(pub, b"", ceremony.sign(priv, b""))

def test_gen_refuses_overwrite(tmp_path):
    cer = tmp_path / "cer"
    assert ceremony.cmd_gen(cer) == 0
    assert ceremony.cmd_gen(cer) == 1

def test_sign_rejects_bad_key(tmp_path):
    cer = tmp_path / "cer"
    cer.mkdir()
    (cer / "lab-privkey.hex").write_text("00" * 16 + "\n")
    (cer / "ceremony.json").write_text("{}")
    out = tmp_path / "out"
    out.mkdir()
    (out / "lab-boletim.json").write_text("{}")
    assert ceremony.cmd_sign(cer, out) == 1

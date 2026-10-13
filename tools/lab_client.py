#!/usr/bin/env python3
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import json
import socket
import struct
import sys
import zlib

def codec(t, run, body: bytes) -> bytes:
    rb, bb = run.encode(), body
    assert len(rb) <= 255 and len(bb) <= 65535
    p = b"\x01" + bytes([t, len(rb)]) + rb + struct.pack(">H", len(bb)) + bb
    return b"NVLP" + p + struct.pack(">I", zlib.crc32(p))

def trans(sock_path: str, t: int, run: str, body: bytes, timeout=5.0):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(sock_path)
    try:
        f = codec(t, run, body)
        s.sendall(struct.pack(">I", len(f)) + f)
        hdr = s.recv(4, socket.MSG_WAITALL)
        (ln,) = struct.unpack(">I", hdr)
        assert ln <= 4 + 1 + 1 + 1 + 255 + 2 + 65535 + 4, "frame excede limite"
        data = s.recv(ln, socket.MSG_WAITALL)
    finally:
        s.close()
    assert data[:4] == b"NVLP", "magic"
    assert data[4] == 1, "versao"
    rtype, rlen = data[5], data[6]
    run_id = data[7 : 7 + rlen].decode()
    blen = struct.unpack(">H", data[7 + rlen : 9 + rlen])[0]
    payload, crc = data[4:-4], struct.unpack(">I", data[-4:])[0]
    assert zlib.crc32(payload) == crc, "crc"
    return rtype, run_id, data[9 + rlen : 9 + rlen + blen]

T = {"HELLO": 1, "LOAD": 3, "RUN": 6, "STATUS": 7, "RESULT": 9, "CANCEL": 11}
NAMES = {4: "ACK", 5: "NAK", 2: "HELLO_ACK", 8: "STATUS", 10: "RESULT"}

def main() -> int:
    sock, cfg_path, run = sys.argv[1], sys.argv[2], sys.argv[3]
    cfg = open(cfg_path, "rb").read()
    t, _, b = trans(sock, T["HELLO"], run, b'{"nonce":"socktest1"}')
    assert t == 2, f"HELLO_ACK esperado, veio {t}: {b!r}"
    print("HELLO_ACK:", json.loads(b)["core_version"])
    t, _, b = trans(sock, T["LOAD"], run, cfg)
    assert t == 4, f"ACK esperado: {b!r}"
    votes = json.dumps(
        {"suite_id": "smoke", "seed": "42",
         "votes": ["alcaide 11", "vereador 11111", "alcaide BRANCO"]}
    ).encode()
    t, _, b = trans(sock, T["RUN"], run, votes)
    assert t == 4, f"ACK esperado: {b!r}"
    print("RUN:", b.decode())
    t, _, b = trans(sock, T["STATUS"], run, b"{}")
    assert t == 8 and b'"done":true' in b, b
    t, _, b = trans(sock, T["RESULT"], run, b"{}")
    assert t == 10, f"RESULT esperado: {b!r}"
    bol = json.loads(b)
    assert "NÃO OFICIAL" in bol["banner"]
    print("RESULT total alcaide:", bol["offices"]["alcaide"]["total"])
    print("SOCKET-E2E OK")
    return 0

if __name__ == "__main__":
    sys.exit(main())

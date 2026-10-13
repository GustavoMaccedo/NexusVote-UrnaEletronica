# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import hashlib

P = 2**255 - 19
L = 2**252 + 27742317777372353535851937790883648493
D = (-121665 * pow(121666, -1, P)) % P
SQRT_M1 = pow(2, (P - 1) // 4, P)
B_Y = (4 * pow(5, -1, P)) % P
B_X = None

def _xrecover(y: int) -> int:
    xx = ((y * y - 1) * pow((D * y * y + 1) % P, -1, P)) % P
    x = pow(xx, (P + 3) // 8, P)
    if (x * x - xx) % P != 0:
        x = (x * SQRT_M1) % P
    if (x * x - xx) % P != 0:
        raise ValueError("ponto fora da curva")
    return x

def _edwards_add(p: tuple, q: tuple) -> tuple:
    (x1, y1), (x2, y2) = p, q
    x3 = ((x1 * y2 + x2 * y1) * pow((1 + D * x1 * x2 * y1 * y2) % P, -1, P)) % P
    y3 = ((y1 * y2 + x1 * x2) * pow((1 - D * x1 * x2 * y1 * y2) % P, -1, P)) % P
    return (x3, y3)

def _scalarmult(pt: tuple, n: int) -> tuple:
    res = ((0, 1))
    addend = pt
    while n:
        if n & 1:
            res = _edwards_add(res, addend)
        addend = _edwards_add(addend, addend)
        n >>= 1
    return res

def _encodepoint(x: int, y: int) -> bytes:
    bits = (x & 1) << 255 | y
    return bits.to_bytes(32, "little")

def _decodepoint(s: bytes) -> tuple:
    if len(s) != 32:
        raise ValueError("ponto deve ter 32 bytes")
    y = int.from_bytes(s, "little") & ((1 << 255) - 1)
    bit = (s[31] >> 7) & 1
    x = _xrecover(y)
    if (x & 1) != bit:
        x = P - x
    return (x, y)

_B = _decodepoint(
    bytes.fromhex("5866666666666666666666666666666666666666666666666666666666666666")
)
B_X, B_Y = _B

def _hint(data: bytes) -> int:
    return int.from_bytes(hashlib.sha512(data).digest(), "little")

def verify(pubkey: bytes, message: bytes, signature: bytes) -> bool:
    try:
        if len(signature) != 64 or len(pubkey) != 32:
            return False
        r_enc, s_enc = signature[:32], signature[32:]
        s_val = int.from_bytes(s_enc, "little")
        if s_val >= L:
            return False
        r_pt = _decodepoint(r_enc)
        a_pt = _decodepoint(pubkey)
        h = _hint(r_enc + pubkey + message) % L
        lhs = _scalarmult(_B, s_val)
        rhs = _edwards_add(r_pt, _scalarmult(a_pt, h))
        return lhs == rhs
    except (ValueError, OverflowError):
        return False

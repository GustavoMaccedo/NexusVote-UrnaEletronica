#!/usr/bin/env python3
# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
import math, wave, struct, os
SR = 44100
HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sounds")
os.makedirs(HERE, exist_ok=True)

def tone(freq=2000.0, dur=0.12, vol=0.9):
    n = int(SR * dur)
    out = bytearray()
    for i in range(n):
        env = 1.0 - (i / n)
        s = math.sin(2 * math.pi * freq * i / SR) * vol * env
        out += struct.pack('<h', int(30000 * s))
    return bytes(out)

def silence(dur):
    return b'\x00\x00' * int(SR * dur)

def seq(pulses, dur, gap, freq=2000.0):
    parts = []
    for i in range(pulses):
        parts.append(tone(freq, dur))
        if i + 1 < pulses:
            parts.append(silence(gap))
    return b''.join(parts)

def write_wav(path, pcm):
    with wave.open(path, 'wb') as w:
        w.setparams((1, 2, SR, len(pcm) // 2, 'NONE', ''))
        w.writeframes(pcm)

if __name__ == '__main__':
    write_wav(os.path.join(HERE, 'confirma.wav'), seq(2, 0.12, 0.18))
    write_wav(os.path.join(HERE, 'fim.wav'), seq(3, 0.25, 0.26))
    print('OK:', sorted(os.listdir(HERE)))

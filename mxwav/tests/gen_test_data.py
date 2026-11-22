#!/usr/bin/env python3
# =============================================================================
# Generate small, self-made MDX/PDX files for mxwav tests (no copyrighted data)
#   fm_a4.mdx      FM ch1: o4a (440Hz) whole note x2, then end
#   fm_octave.mdx  FM ch1: o3a then o5a (octave check)
#   loop.mdx       FM ch1: infinite loop of a short phrase (loop/fade-out test)
#   adpcm.mdx      ADPCM ch(P): PDX sample #0 (1kHz sine, MSM6258 ADPCM)
#   pcm8.mdx       PCM8 mode (E8): ch P at 0s, ch Q at 1s (Q only audible with PCM8)
#   TEST.PDX       PDX referenced as "test" from adpcm.mdx (case-insensitive lookup)
# Usage: gen_test_data.py OUTDIR
# =============================================================================
import math, os, struct, sys

def voice(no):
    # 27 bytes: no, FL/CON, slot mask, DT1MUL x4, TL x4, KSAR x4, AMED1R x4, DT2D2R x4, D1LRR x4
    # CON=7 (all carriers), only C2 audible -> pure sine
    con = 7
    return bytes([no, (0 << 3) | con, 0x0F,
                  1, 1, 1, 1,             # DT1=0 MUL=1
                  127, 127, 127, 0,       # TL: M1 C1 M2 silent, C2 full
                  0x1F, 0x1F, 0x1F, 0x1F, # AR max
                  0, 0, 0, 0,             # D1R
                  0, 0, 0, 0,             # D2R
                  0x0F, 0x0F, 0x0F, 0x0F])  # D1L=0 RR=15

# MDX note: 0x80 = o0d+ ; note number n -> 0x80+n
NOTE_NAMES = {'c': -3, 'c+': -2, 'd': -1, 'd+': 0, 'e': 1, 'f': 2, 'f+': 3, 'g': 4, 'g+': 5, 'a': 6, 'a+': 7, 'b': 8}
def note(octave, name, length):
    n = octave * 12 + NOTE_NAMES[name]
    assert 0 <= n < 0x60 and 1 <= length <= 256
    return bytes([0x80 + n, length - 1])

def rest(length):
    out = b''
    while length > 0:
        l = min(length, 128)
        out += bytes([l - 1]); length -= l
    return out

def mdx(title, pdx, tracks, voices):
    """tracks: list of 16 byte strings (FM1-8, ADPCM, PCM8 2-8)."""
    head = title.encode('cp932') + b'\r\n\x1a' + (pdx.encode('cp932') if pdx else b'') + b'\x00'
    nch = 16
    table_size = 2 + 2 * nch
    body = b''
    offsets = []
    for t in tracks:
        offsets.append(table_size + len(body))
        body += t
    voice_ofs = table_size + len(body)
    body += b''.join(voices)
    table = struct.pack('>H', voice_ofs) + b''.join(struct.pack('>H', o) for o in offsets)
    return head + table + body

END = b'\xf1\x00'
TEMPO = lambda t: bytes([0xFF, t])
VOICE = lambda v: bytes([0xFD, v])
VOL = lambda v: bytes([0xFB, v])
PAN = lambda p: bytes([0xFC, p])
# Timer-B value: tempo(bpm) = 4000000*60/(1024*(256-t)*48)  (48 clocks per quarter)
T120 = 256 - round(4000000 * 60 / (1024 * 120 * 48))

def fm_track(body):
    return TEMPO(T120) + VOICE(0) + VOL(15) + PAN(3) + body + END

def silent():
    return END

def write(path, data):
    with open(path, 'wb') as f:
        f.write(data)

# ---- MSM6258 (OKI) ADPCM encoder --------------------------------------------
STEP = [16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157,
        173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
        1282, 1411, 1552]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8]
def adpcm_encode(samples12):
    pred, idx, out = 0, 0, []
    for s in samples12:
        step = STEP[idx]
        diff = s - pred
        code = 0
        if diff < 0: code = 8; diff = -diff
        if diff >= step: code |= 4; diff -= step
        if diff >= step >> 1: code |= 2; diff -= step >> 1
        if diff >= step >> 2: code |= 1
        # decoder reconstruction
        d = step >> 3
        if code & 4: d += step
        if code & 2: d += step >> 1
        if code & 1: d += step >> 2
        pred = pred - d if code & 8 else pred + d
        pred = max(-2048, min(2047, pred))
        idx = max(0, min(48, idx + IDX[code & 7]))
        out.append(code)
    b = bytearray()
    for i in range(0, len(out) - 1, 2):
        b.append(out[i] | (out[i + 1] << 4))   # low nibble first
    return bytes(b)

def pdx(samples):
    """samples: list of bytes (ADPCM data). 96 entries x (offset32, length32)."""
    table = bytearray(96 * 8)
    data = b''
    for i, s in enumerate(samples):
        struct.pack_into('>II', table, i * 8, 96 * 8 + len(data), len(s))
        data += s
    return bytes(table) + data

def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    v = [voice(0)]
    # quarter = 48 clocks; whole = 192
    write(os.path.join(outdir, 'fm_a4.mdx'),
          mdx('テスト A4 サイン波', '', [fm_track(note(4, 'a', 192) + note(4, 'a', 192))] + [silent()] * 15, v))
    write(os.path.join(outdir, 'fm_octave.mdx'),
          mdx('octave test', '', [fm_track(note(3, 'a', 192) + note(5, 'a', 192))] + [silent()] * 15, v))
    # loop: 1 bar (c e g c), loop forever
    phrase = note(4, 'c', 48) + note(4, 'e', 48) + note(4, 'g', 48) + note(5, 'c', 48)
    head = TEMPO(T120) + VOICE(0) + VOL(15) + PAN(3)
    # F1 + word: jump back by (distance from loop start to position after the word)
    back = len(phrase) + 3
    loop_track = head + phrase + b'\xf1' + struct.pack('>H', (0x10000 - back) & 0xFFFF)
    write(os.path.join(outdir, 'loop.mdx'),
          mdx('loop test', '', [loop_track] + [silent()] * 15, v))
    # ADPCM: 1kHz sine @15625Hz, 0.8s, sample #0 ; channel P plays note index 0 twice
    sr = 15625
    sine = [int(1500 * math.sin(2 * math.pi * 1000 * i / sr)) for i in range(int(sr * 0.8))]
    write(os.path.join(outdir, 'TEST.PDX'), pdx([adpcm_encode(sine)]))
    pcm_track = TEMPO(T120) + VOL(15) + PAN(3) + bytes([0x80 + 0, 96 - 1]) + rest(96) + bytes([0x80 + 0, 96 - 1]) + rest(96) + END
    tracks = [silent()] * 8 + [pcm_track] + [silent()] * 7
    write(os.path.join(outdir, 'adpcm.mdx'), mdx('adpcm test', 'test', tracks, v))
    # PCM8: E8 (PCM8 mode) on ch P; ch Q plays sample #0 starting at 1.0s
    # (v8: v15 overdrives the PCM8 mix into clipping, also with the original X68Sound)
    p_track = TEMPO(T120) + b'\xe8' + VOL(8) + PAN(3) + bytes([0x80, 48 - 1]) + rest(192) + END
    q_track = VOL(8) + PAN(3) + rest(96) + bytes([0x80, 48 - 1]) + rest(96) + END
    tracks = [silent()] * 8 + [p_track, q_track] + [silent()] * 6
    write(os.path.join(outdir, 'pcm8.mdx'), mdx('pcm8 test', 'test', tracks, v))
    print('T120 timer-B =', T120)

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'testdata')

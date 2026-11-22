#!/usr/bin/env python3
# WAV analysis helper for mxwav tests (pure Python, no numpy needed)
#   check_wav.py info FILE                       -> rate ch bits seconds rms
#   check_wav.py freq FILE START END             -> dominant frequency (Hz) in [START,END) seconds
#   check_wav.py rms FILE START END              -> RMS in [START,END) seconds (left channel)
#   check_wav.py thd FILE START END F0           -> harmonic distortion 2nd..5th (%)
#   check_wav.py thdn FILE START END F0          -> THD+N (%) (residual after sine fit)
#   check_wav.py noise FILE                      -> noise index (2nd-difference energy ratio)
import math, struct, sys

def load(path):
    with open(path, 'rb') as f:
        d = f.read()
    assert d[0:4] == b'RIFF' and d[8:12] == b'WAVE', 'not a WAV file'
    p = 12; fmt = None; data = None
    while p + 8 <= len(d):
        cid, size = d[p:p + 4], struct.unpack('<I', d[p + 4:p + 8])[0]
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', d[p + 8:p + 24])
        elif cid == b'data':
            data = d[p + 8:p + 8 + size]
            assert len(data) == size, 'data chunk size mismatch (%d != %d)' % (len(data), size)
        p += 8 + size
    tag, ch, rate, bps, align, bits = fmt
    assert tag == 1 and bits == 16 and align == ch * 2 and bps == rate * align, 'bad fmt chunk'
    n = len(data) // 2
    s = struct.unpack('<%dh' % n, data[:n * 2])
    left = s[0::ch]
    return rate, ch, bits, left, s

def seg(x, rate, a, b):
    return x[int(a * rate):int(b * rate)]

def goertzel(x, rate, f):
    w = 2 * math.pi * f / rate; c = 2 * math.cos(w); s1 = s2 = 0.0
    for v in x:
        s0 = v + c * s1 - s2; s2 = s1; s1 = s0
    return (s1 * s1 + s2 * s2 - c * s1 * s2) * 2 / (len(x) ** 2)

def peak(x, rate, lo=50, hi=4000):
    x = x[:min(len(x), int(rate * 0.25))]
    # decimate for speed
    dec = max(1, rate // 8000)
    xs = x[::dec]; r = rate / dec
    best, bp = lo, -1
    f = lo
    while f <= min(hi, r / 2 - 1):
        p = goertzel(xs, r, f)
        if p > bp: bp, best = p, f
        f *= 1.01
    lo2, hi2 = best / 1.02, best * 1.02
    f = lo2
    while f <= hi2:
        p = goertzel(xs, r, f)
        if p > bp: bp, best = p, f
        f += best * 0.0005
    return best

def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0

def noise(x):
    e = d = 0.0
    for i in range(2, len(x)):
        dd = x[i] - 2 * x[i - 1] + x[i - 2]
        d += dd * dd; e += x[i] * x[i]
    return d / e if e else 0.0

cmd, path = sys.argv[1], sys.argv[2]
rate, ch, bits, left, alls = load(path)
if cmd == 'info':
    print(rate, ch, bits, '%.3f' % (len(left) / rate), '%.1f' % rms(left))
elif cmd == 'freq':
    print('%.2f' % peak(seg(left, rate, float(sys.argv[3]), float(sys.argv[4])), rate))
elif cmd == 'rms':
    print('%.1f' % rms(seg(left, rate, float(sys.argv[3]), float(sys.argv[4]))))
elif cmd == 'thd':
    x = seg(left, rate, float(sys.argv[3]), float(sys.argv[4])); f0 = float(sys.argv[5])
    a1 = math.sqrt(goertzel(x, rate, f0))
    h = math.sqrt(sum(goertzel(x, rate, f0 * k) for k in range(2, 6) if f0 * k < rate / 2))
    print('%.3f' % (h / a1 * 100 if a1 else 999))
elif cmd == 'thdn':
    x = seg(left, rate, float(sys.argv[3]), float(sys.argv[4])); f0 = float(sys.argv[5]); n = len(x)
    w = 2 * math.pi * f0 / rate
    a = sum(v * math.sin(w * i) for i, v in enumerate(x)) * 2 / n
    b = sum(v * math.cos(w * i) for i, v in enumerate(x)) * 2 / n
    r = math.sqrt(sum((v - (a * math.sin(w * i) + b * math.cos(w * i))) ** 2 for i, v in enumerate(x)) / n)
    print('%.4f' % (r / (math.hypot(a, b) / math.sqrt(2)) * 100))
elif cmd == 'noise':
    print('%.3e' % noise(left))

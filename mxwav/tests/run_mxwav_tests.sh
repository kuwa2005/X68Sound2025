#!/bin/bash
# =============================================================================
# mxwav end-to-end tests (Linux build)
#   Generates self-made MDX/PDX files, converts them with mxwav and checks the
#   resulting WAV files (format, length, pitch, ADPCM, masks, options).
# Usage: mxwav/tests/run_mxwav_tests.sh   (after ./build.sh)
# =============================================================================
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
MXWAV="${MXWAV:-$HERE/../build/linux/mxwav}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
PY="python3 -I"
CHK="$PY $HERE/check_wav.py"

pass=0; fail=0
ok()   { echo "    PASS: $*"; pass=$((pass+1)); }
ng()   { echo "    FAIL: $*"; fail=$((fail+1)); }
# float compare: within a b tol(ratio)
near() { $PY -c "import sys;a,b,t=map(float,sys.argv[1:]);sys.exit(0 if abs(a-b)<=abs(b)*t else 1)" "$1" "$2" "$3"; }
lt()   { $PY -c "import sys;a,b=map(float,sys.argv[1:]);sys.exit(0 if a<b else 1)" "$1" "$2"; }
run()  { (cd "$WORK" && env "$@" timeout 120 "$MXWAV" "${ARGS[@]}" >"$WORK/out.txt" 2>&1); }

[ -x "$MXWAV" ] || { echo "mxwav not built: $MXWAV"; exit 1; }
$PY "$HERE/gen_test_data.py" "$WORK" >/dev/null

echo "[TEST] FM tone: WAV format, length, pitch (o4a = 440Hz)"
ARGS=(fm_a4.mdx); run X=1
read rate chn bits sec r <<<"$($CHK info "$WORK/fm_a4.wav")"
[ "$rate $chn $bits" = "44100 2 16" ] && ok "format $rate/$chn/$bits" || ng "format $rate/$chn/$bits"
near "$sec" 6.0 0.03 && ok "length ${sec}s (4s music + 2s wait)" || ng "length ${sec}s"
f=$($CHK freq "$WORK/fm_a4.wav" 0.5 1.5); near "$f" 440 0.01 && ok "pitch ${f}Hz" || ng "pitch ${f}Hz (expected 440)"
n=$($CHK noise "$WORK/fm_a4.wav"); lt "$n" 0.01 && ok "clean (noise $n)" || ng "noisy (noise $n)"
grep -q "TITLE: テスト A4 サイン波" "$WORK/out.txt" && ok "Shift_JIS title shown as UTF-8" || ng "title: $(grep TITLE "$WORK/out.txt")"

echo "[TEST] FM octave: o3a -> o5a"
ARGS=(fm_octave.mdx); run X=1
f1=$($CHK freq "$WORK/fm_octave.wav" 0.5 1.5); f2=$($CHK freq "$WORK/fm_octave.wav" 2.5 3.5)
near "$f1" 220 0.01 && near "$f2" 880 0.01 && ok "o3a=${f1}Hz o5a=${f2}Hz" || ng "o3a=${f1}Hz o5a=${f2}Hz"

echo "[TEST] Loop + fade-out (-L1 / -L3)"
ARGS=(-L1 -W0 -Oloop1.wav loop.mdx); run X=1; r1=$?
ARGS=(-L3 -W0 -Oloop3.wav loop.mdx); run X=1; r3=$?
read _ _ _ s1 _ <<<"$($CHK info "$WORK/loop1.wav")"; read _ _ _ s3 _ <<<"$($CHK info "$WORK/loop3.wav")"
[ $r1 -eq 0 ] && [ $r3 -eq 0 ] && ok "infinite loop terminates" || ng "did not terminate (rc $r1/$r3)"
d=$($PY -c "print(round($s3-$s1,2))"); near "$d" 4.0 0.05 && ok "-L3 is 2 loops (4.0s) longer than -L1: ${s1}s / ${s3}s" || ng "loop lengths ${s1}s / ${s3}s (diff $d, expected 4.0)"
a=$($CHK rms "$WORK/loop3.wav" 1 3); b=$($CHK rms "$WORK/loop3.wav" $($PY -c "print($s3-1.0)") $s3)
lt "$b" "$($PY -c "print($a*0.3)")" && ok "fades out (rms $a -> $b)" || ng "no fade-out (rms $a -> $b)"

echo "[TEST] ADPCM single channel (MSM6258 via DMA; with and without -U)"
for mode in pcm8 dma; do
	if [ $mode = pcm8 ]; then ARGS=(-Oadpcm_$mode.wav adpcm.mdx); else ARGS=(-U -Oadpcm_$mode.wav adpcm.mdx); fi
	run X=1; rc=$?
	if grep -q "見つかりません" "$WORK/out.txt"; then ng "$mode: PDX not found (case-insensitive lookup)"; continue; fi
	r=$($CHK rms "$WORK/adpcm_$mode.wav" 0.1 0.6); f=$($CHK freq "$WORK/adpcm_$mode.wav" 0.1 0.6)
	[ $rc -eq 0 ] && lt 300 "$r" && near "$f" 1000 0.01 && ok "$mode: 1kHz ADPCM played (rms $r, ${f}Hz)" || ng "$mode: rc=$rc rms=$r f=${f}Hz"
	s=$($CHK rms "$WORK/adpcm_$mode.wav" 1.05 1.45); lt "$s" 50 && ok "$mode: rest is silent (rms $s)" || ng "$mode: rest not silent (rms $s)"
	t=$($CHK thd "$WORK/adpcm_$mode.wav" 0.1 0.6 1000.21); lt "$t" 2 && ok "$mode: undistorted (THD $t%)" || ng "$mode: distorted ADPCM (THD $t%)"
done

echo "[TEST] PCM8 multi-channel (E8): ch Q audible only with PCM8 (default), not with -U"
ARGS=(-Opcm8_on.wav pcm8.mdx); run X=1
ARGS=(-U -Opcm8_off.wav pcm8.mdx); run X=1
p=$($CHK rms "$WORK/pcm8_on.wav" 0.05 0.4); q=$($CHK rms "$WORK/pcm8_on.wav" 1.05 1.4); fq=$($CHK freq "$WORK/pcm8_on.wav" 1.05 1.4)
lt 300 "$p" && lt 300 "$q" && near "$fq" 1000 0.01 && ok "PCM8: ch P (rms $p) and ch Q (rms $q, ${fq}Hz) play" || ng "PCM8: P rms $p, Q rms $q, ${fq}Hz"
t=$($CHK thd "$WORK/pcm8_on.wav" 1.05 1.4 1000.21); lt "$t" 2 && ok "PCM8: undistorted (THD $t%)" || ng "PCM8: distorted (THD $t%)"
p=$($CHK rms "$WORK/pcm8_off.wav" 0.05 0.4); q=$($CHK rms "$WORK/pcm8_off.wav" 1.05 1.4)
lt 300 "$p" && lt "$q" 10 && ok "-U: ch P plays via MSM6258 (rms $p), ch Q silent (rms $q)" || ng "-U: P rms $p, Q rms $q"

echo "[TEST] AI ADPCM enhancement (-A, offline)"
ARGS=(-Oai_off.wav adpcm.mdx); run X68SOUND_PCM_RESAMPLE=1
ARGS=(-A -Oai_on.wav adpcm.mdx); run X68SOUND_PCM_RESAMPLE=1
t0=$($CHK thdn "$WORK/ai_off.wav" 0.1 0.6 1000.0); t1=$($CHK thdn "$WORK/ai_on.wav" 0.1 0.6 1000.0)
r0=$($CHK rms "$WORK/ai_off.wav" 0.1 0.6); r1=$($CHK rms "$WORK/ai_on.wav" 0.1 0.6)
lt "$t1" "$($PY -c "print($t0*0.8)")" && near "$r1" "$r0" 0.06 && ok "-A reduces ADPCM THD+N $t0% -> $t1% (rms $r0 -> $r1)" || ng "-A: THD+N $t0% -> $t1%, rms $r0 -> $r1"

echo "[TEST] Channel mask (-M)"
ARGS=(-M1 -Omask.wav fm_a4.mdx); run X=1
r=$($CHK rms "$WORK/mask.wav" 0.5 3.5); lt "$r" 10 && ok "-M1 mutes FM ch1 (rms $r)" || ng "-M1 rms $r"
ARGS=(-MP -Omaskp.wav adpcm.mdx); run X=1
r=$($CHK rms "$WORK/maskp.wav" 0.1 0.6); lt "$r" 10 && ok "-MP mutes ADPCM (rms $r)" || ng "-MP rms $r"

echo "[TEST] Sampling rates (-S / X68SOUND_OUTPUT_RATE)"
for sr in 22050 48000 96000 192000; do
	ARGS=(-S$sr -Osr$sr.wav fm_a4.mdx); run X=1
	read rate _ _ sec _ <<<"$($CHK info "$WORK/sr$sr.wav")"; f=$($CHK freq "$WORK/sr$sr.wav" 0.5 1.0)
	[ "$rate" = "$sr" ] && near "$sec" 6.0 0.03 && near "$f" 440 0.01 && ok "-S$sr: header $rate, ${sec}s, ${f}Hz" || ng "-S$sr: header $rate, ${sec}s, ${f}Hz"
done
ARGS=(-Oenv96.wav fm_a4.mdx); run X68SOUND_OUTPUT_RATE=96000
read rate _ _ sec _ <<<"$($CHK info "$WORK/env96.wav")"; f=$($CHK freq "$WORK/env96.wav" 0.5 1.0)
[ "$rate" = "96000" ] && near "$sec" 6.0 0.03 && near "$f" 440 0.01 && ok "X68SOUND_OUTPUT_RATE=96000 -> header $rate, ${sec}s, ${f}Hz" || ng "env rate: header $rate, ${sec}s, ${f}Hz"

echo "[TEST] X68Sound2025 effects via environment"
ARGS=(-Ofx.wav fm_a4.mdx); run X68SOUND_REVERB=2 X68SOUND_EXCITER=2 X68SOUND_SUB_BASS=2 X68SOUND_STEREO_WIDTH=150 X68SOUND_CROSSFEED=30 X68SOUND_HAAS_EFFECT=50
n=$($CHK noise "$WORK/fx.wav"); a=$($CHK rms "$WORK/fm_a4.wav" 4.2 5.5); b=$($CHK rms "$WORK/fx.wav" 4.2 5.5)
lt "$n" 0.01 && ok "effects clean (noise $n)" || ng "effects noisy ($n)"
lt "$a" "$b" && ok "reverb tail present after note end (rms dry $a, wet $b)" || ng "no reverb tail (dry $a, wet $b) - env not applied?"

echo "[TEST] Options -B (benchmark) / errors"
ARGS=(-B -Obench.wav fm_a4.mdx); run X=1
[ ! -e "$WORK/bench.wav" ] && grep -q "生成速度" "$WORK/out.txt" && ok "-B writes no file" || ng "-B"
ARGS=(nonexist.mdx); run X=1; rc=$?
[ $rc -ne 0 ] && grep -q "読み込みに失敗" "$WORK/out.txt" && ok "missing MDX -> error" || ng "missing MDX rc=$rc"
printf 'not an mdx' > "$WORK/bad.mdx"; ARGS=(bad.mdx); run X=1; rc=$?
[ $rc -ne 0 ] && grep -q "壊れています" "$WORK/out.txt" && ok "broken MDX -> error" || ng "broken MDX rc=$rc"

echo "============================================"
echo "mxwav tests: $pass passed, $fail failed"
echo "============================================"
[ $fail -eq 0 ]

// =============================================================================
// X68Sound2025 - Integration tests (real source, offline rendering)
// =============================================================================
// Builds the ACTUAL X68Sound.cpp (not copies of functions) on Linux using the
// Win32 compatibility layer in compat/linux, renders audio with X68Sound_StartPcm/GetPcm,
// and checks that each option (and combinations of options) produces the
// intended output - in particular that no combination produces
// "random-noise like" destroyed audio.
//
// Only the public DLL API + environment variables are used, so this file also
// builds against older revisions (useful to confirm the tests catch the bugs).
//
// Build & run: tests/run_tests.sh
// =============================================================================

#include "../X68Sound/X68Sound.cpp"

#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// -----------------------------------------------------------------------------
// Minimal test framework
// -----------------------------------------------------------------------------
static int g_run = 0, g_pass = 0, g_fail = 0;
static bool g_cur_ok;

#define CHECK(cond, ...) do { \
	if (!(cond)) { printf("    FAIL: "); printf(__VA_ARGS__); printf("  [%s:%d]\n", __FILE__, __LINE__); g_cur_ok = false; } \
} while (0)

#define INFO(...) do { printf("    "); printf(__VA_ARGS__); printf("\n"); } while (0)

#define RUN(fn) do { printf("[TEST] %s\n", #fn); g_run++; g_cur_ok = true; fn(); \
	if (g_cur_ok) { g_pass++; printf("    PASS\n"); } else { g_fail++; } } while (0)

// -----------------------------------------------------------------------------
// Environment helpers
// -----------------------------------------------------------------------------
static const char *ALL_VARS[] = {
	"X68SOUND_PCM_BUFFER", "X68SOUND_LINEAR_INTERPOLATION", "X68SOUND_VOLUME_SMOOTHING",
	"X68SOUND_OPM_SINE_INTERP", "X68SOUND_OUTPUT_RATE", "X68SOUND_ADPCM_MODE",
	"X68SOUND_EXCITER", "X68SOUND_SUB_BASS", "X68SOUND_FM_HARMONIC", "X68SOUND_FM_WARMTH",
	"X68SOUND_FM_CHORUS", "X68SOUND_STEREO_WIDTH", "X68SOUND_CROSSFEED", "X68SOUND_CROSSFEED_DELAY",
	"X68SOUND_CENTER_WIDTH", "X68SOUND_HAAS_EFFECT", "X68SOUND_HAAS_DELAY",
	"X68SOUND_EARLY_REFLECTIONS", "X68SOUND_REVERB", "X68SOUND_REVERB_DECAY", "X68SOUND_REVERB_MIX",
	"X68SOUND_COMPRESSOR", "X68SOUND_FM_OCTAVE_UPPER", "X68SOUND_FM_OCTAVE_UPPER_VOL",
	"X68SOUND_FM_OCTAVE_LOWER", "X68SOUND_FM_OCTAVE_LOWER_VOL", "X68SOUND_ADPCM_OCTAVE_UPPER",
	"X68SOUND_ADPCM_OCTAVE_LOWER", "X68SOUND_ADPCM_OCTAVE_LOWER2", "X68SOUND_ADPCM_MULTICHANNEL",
	"X68SOUND_OCTAVE_AUTO_GAIN", "X68SOUND_FM_MASTER_VOLUME", "X68SOUND_ADPCM_MASTER_VOLUME",
	"X68SOUND_SOFT_CLIPPING", "X68SOUND_SOFT_CLIPPING_THRESHOLD", "X68SOUND_HQ_SINE",
	"X68SOUND_LFSR_NOISE", "X68SOUND_DITHERING", "X68SOUND_DEBUG",
	"X68SOUND_PCM_RESAMPLE", "X68SOUND_REVERB_ALGO", "X68SOUND_REVERB_PREDELAY", "X68SOUND_CHORUS_ALGO",
	"X68SOUND_EQ_LOW_GAIN", "X68SOUND_EQ_LOW_FREQ", "X68SOUND_EQ_MID_GAIN", "X68SOUND_EQ_MID_FREQ", "X68SOUND_EQ_MID_Q",
	"X68SOUND_EQ_HIGH_GAIN", "X68SOUND_EQ_HIGH_FREQ", "X68SOUND_LIMITER", "X68SOUND_LIMITER_CEILING",
	"X68SOUND_AI_ENHANCE", "X68SOUND_AI_STRENGTH",
	NULL
};

// Set environment: list of "NAME=VALUE" (NAME without X68SOUND_ prefix)
static void SetEnv(std::initializer_list<const char *> kv) {
	for (int i = 0; ALL_VARS[i]; ++i) unsetenv(ALL_VARS[i]);
	for (const char *s : kv) {
		std::string str(s);
		size_t eq = str.find('=');
		std::string name = "X68SOUND_" + str.substr(0, eq);
		setenv(name.c_str(), str.substr(eq + 1).c_str(), 1);
	}
	LoadConfigFromEnvironment();
}

// -----------------------------------------------------------------------------
// Rendering helpers
// -----------------------------------------------------------------------------
struct Stereo {
	std::vector<double> L, R;
	int rate;
};

static void Opm(int reg, int data) {
	X68Sound_OpmReg((unsigned char)reg);
	X68Sound_OpmPoke((unsigned char)data);
}

static void Start(int rate) {
	X68Sound_Free();
	X68Sound_WaveFunc(NULL);
	int r = X68Sound_StartPcm(rate, 1, 1, 5);
	if (r != 0) printf("    (StartPcm returned %d)\n", r);
	// All channels: silent
	for (int ch = 0; ch < 8; ++ch) {
		Opm(0x08, ch);  // key off
		for (int s = 0; s < 4; ++s) Opm(0x60 + ch + s * 8, 127);
	}
}

static Stereo Render(int rate, double seconds, double skip = 0.0) {
	Stereo out;
	out.rate = rate;
	int nskip = (int)(rate * skip);
	int n = (int)(rate * seconds);
	std::vector<short> buf(2 * 4096);
	int done = 0;
	while (done < nskip + n) {
		int chunk = std::min(4096, nskip + n - done);
		X68Sound_GetPcm(buf.data(), chunk);
		for (int i = 0; i < chunk; ++i) {
			if (done + i >= nskip) {
				out.L.push_back(buf[2 * i]);
				out.R.push_back(buf[2 * i + 1]);
			}
		}
		done += chunk;
	}
	return out;
}

// Single sine-like FM tone on channel ch: algorithm 7, only one operator audible.
static void FmTone(int ch, int kc, int tl = 0, int pan = 0xC0, int fb = 0, int mul = 1) {
	Opm(0x20 + ch, pan | (fb << 3) | 7);  // RL, FB, CON=7
	Opm(0x28 + ch, kc);
	Opm(0x30 + ch, 0);
	Opm(0x38 + ch, 0);
	for (int s = 0; s < 4; ++s) {
		int off = ch + s * 8;
		Opm(0x40 + off, mul & 15);         // DT1=0, MUL
		Opm(0x60 + off, (s == 3) ? tl : 127);
		Opm(0x80 + off, 0x1F);             // AR max
		Opm(0xA0 + off, 0);                // D1R 0
		Opm(0xC0 + off, 0);                // D2R 0
		Opm(0xE0 + off, 0x0F);             // D1L 0, RR 15
	}
	Opm(0x08, 0x78 | ch);                  // key on all slots
}

// -----------------------------------------------------------------------------
// Analysis helpers
// -----------------------------------------------------------------------------
static double Rms(const std::vector<double> &x) {
	double e = 0; for (double v : x) e += v * v;
	return x.empty() ? 0 : sqrt(e / x.size());
}
static double Peak(const std::vector<double> &x) {
	double m = 0; for (double v : x) m = std::max(m, fabs(v)); return m;
}
// Power at frequency f (Goertzel, normalized so that a sine of amplitude A gives A^2/2)
static double PowerAt(const std::vector<double> &x, int rate, double f) {
	double w = 2 * M_PI * f / rate, c = 2 * cos(w), s1 = 0, s2 = 0;
	for (double v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
	double p = s1 * s1 + s2 * s2 - c * s1 * s2;
	return 2.0 * p / ((double)x.size() * x.size());
}
// Strongest frequency in [lo, hi] (coarse+fine search)
static double PeakFreq(const std::vector<double> &x, int rate, double lo, double hi) {
	std::vector<double> seg(x.begin(), x.begin() + std::min<size_t>(x.size(), rate / 2));
	double best = lo, bp = -1;
	for (double f = lo; f <= hi; f += 2.0) { double p = PowerAt(seg, rate, f); if (p > bp) { bp = p; best = f; } }
	double b2 = best; bp = -1;
	for (double f = best - 2; f <= best + 2; f += 0.1) { double p = PowerAt(seg, rate, f); if (p > bp) { bp = p; b2 = f; } }
	return b2;
}
// "Noise index": energy of 2nd difference relative to signal energy.
// For band-limited music-like signals this is tiny (<1e-2); random sign flips
// / wrap-around garbage push it towards 1..16.
static double NoiseIndex(const std::vector<double> &x) {
	double e = 0, d = 0;
	for (size_t i = 2; i < x.size(); ++i) {
		double dd = x[i] - 2 * x[i - 1] + x[i - 2];
		d += dd * dd; e += x[i] * x[i];
	}
	return e > 0 ? d / e : 0;
}
static double MaxJump(const std::vector<double> &x) {
	double m = 0; for (size_t i = 1; i < x.size(); ++i) m = std::max(m, fabs(x[i] - x[i - 1])); return m;
}
static int ClipCount(const std::vector<double> &x) {
	int c = 0; for (double v : x) if (fabs(v) >= 32767) ++c; return c;
}
static double RmsDiff(const std::vector<double> &a, const std::vector<double> &b) {
	double e = 0; size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n; ++i) e += (a[i] - b[i]) * (a[i] - b[i]);
	return n ? sqrt(e / n) : 0;
}
static double Corr(const std::vector<double> &a, const std::vector<double> &b) {
	double ab = 0, aa = 0, bb = 0; size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n; ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
	return (aa > 0 && bb > 0) ? ab / sqrt(aa * bb) : 0;
}

// Reference tone: A (KC=0x4A) - measured once
static const int KC_A4 = 0x4A;
static double g_f0 = 0;

static Stereo RenderTone(int rate, int kc = KC_A4, int tl = 0, double sec = 1.0, int nch = 1) {
	Start(rate);
	for (int ch = 0; ch < nch; ++ch) FmTone(ch, kc, tl);
	return Render(rate, sec, 0.2);
}

// -----------------------------------------------------------------------------
// WaveFunc signal injection (enters the master mix before the effect chain)
// -----------------------------------------------------------------------------
static std::vector<int> g_wave;   // packed (R<<16)|(L&0xFFFF)
static size_t g_wave_pos = 0;
static int CALLBACK WaveSrc() {
	if (g_wave_pos < g_wave.size()) return g_wave[g_wave_pos++];
	return 0;
}
static int Pack(int l, int r) { return (int)(((unsigned)(r & 0xFFFF) << 16) | (unsigned)(l & 0xFFFF)); }

static Stereo RenderWave(int rate, const std::vector<int> &wave) {
	Start(rate);
	g_wave = wave; g_wave_pos = 0;
	X68Sound_WaveFunc(WaveSrc);
	Stereo s = Render(rate, (double)wave.size() / rate);
	X68Sound_WaveFunc(NULL);
	return s;
}

static std::vector<int> SineWave(int rate, double f, double amp, double sec, double ampR = -1) {
	if (ampR < 0) ampR = amp;
	std::vector<int> w;
	for (int i = 0; i < (int)(rate * sec); ++i) {
		double v = sin(2 * M_PI * f * i / rate);
		w.push_back(Pack((int)(amp * v), (int)(ampR * v)));
	}
	return w;
}

// =============================================================================
// Tests
// =============================================================================

// --- Configuration ---------------------------------------------------------

// "0" must be accepted for options whose default is ON (e.g. SOFT_CLIPPING=0).
static void test_config_zero_values_are_respected() {
	SetEnv({"SOFT_CLIPPING=0", "LINEAR_INTERPOLATION=0", "VOLUME_SMOOTHING=0",
	        "OPM_SINE_INTERP=0", "OCTAVE_AUTO_GAIN=0", "FM_MASTER_VOLUME=0"});
	CHECK(g_Config.soft_clipping_enable == 0, "SOFT_CLIPPING=0 -> %d", g_Config.soft_clipping_enable);
	CHECK(g_Config.linear_interpolation == 0, "LINEAR_INTERPOLATION=0 -> %d", g_Config.linear_interpolation);
	CHECK(g_Config.volume_smoothing == 0, "VOLUME_SMOOTHING=0 -> %d", g_Config.volume_smoothing);
	CHECK(g_Config.opm_sine_interpolation == 0, "OPM_SINE_INTERP=0 -> %d", g_Config.opm_sine_interpolation);
	CHECK(g_Config.octave_auto_gain == 0, "OCTAVE_AUTO_GAIN=0 -> %d", g_Config.octave_auto_gain);
	CHECK(g_Config.fm_master_volume == 0, "FM_MASTER_VOLUME=0 -> %d", g_Config.fm_master_volume);
}

static void test_config_defaults_and_clamping() {
	SetEnv({});
	CHECK(g_Config.soft_clipping_enable == 1, "default SOFT_CLIPPING");
	CHECK(g_Config.stereo_width == 100, "default STEREO_WIDTH");
	CHECK(g_Config.hq_sine_table == 0 && g_Config.lfsr_noise == 0, "HQ/LFSR default OFF");
	SetEnv({"REVERB=9", "STEREO_WIDTH=999", "EXCITER=99", "FM_CHORUS=7", "CROSSFEED_DELAY=50", "abc=1"});
	CHECK(g_Config.reverb_type == 4, "REVERB clamp -> %d", g_Config.reverb_type);
	CHECK(g_Config.stereo_width == 200, "STEREO_WIDTH clamp -> %d", g_Config.stereo_width);
	CHECK(g_Config.exciter_level == 4, "EXCITER clamp -> %d", g_Config.exciter_level);
	CHECK(g_Config.fm_chorus_depth == 3, "FM_CHORUS clamp -> %d", g_Config.fm_chorus_depth);
	CHECK(g_Config.crossfeed_delay == 10, "CROSSFEED_DELAY clamp -> %d", g_Config.crossfeed_delay);
	SetEnv({"REVERB=abc"});
	CHECK(g_Config.reverb_type == 0, "non-numeric -> default (%d)", g_Config.reverb_type);
}

// --- FM synthesis --------------------------------------------------------------

static void test_fm_default_tone_is_clean() {
	SetEnv({});
	Stereo s = RenderTone(48000);
	g_f0 = PeakFreq(s.L, 48000, 100, 2000);
	double purity = PowerAt(s.L, 48000, g_f0) / (Rms(s.L) * Rms(s.L));
	INFO("f0=%.2fHz rms=%.0f purity=%.4f noise=%.2e", g_f0, Rms(s.L), purity, NoiseIndex(s.L));
	CHECK(Rms(s.L) > 1000, "tone too quiet (rms=%.0f)", Rms(s.L));
	CHECK(g_f0 > 300 && g_f0 < 700, "unexpected f0 %.1f", g_f0);
	CHECK(purity > 0.98, "tone not pure: %.4f", purity);
	CHECK(Corr(s.L, s.R) > 0.999, "L/R should be identical for center pan");
}

static void test_fm_pitch_is_independent_of_sample_rate() {
	SetEnv({});
	int rates[] = {44100, 48000, 96000, 192000};
	for (int r : rates) {
		Stereo s = RenderTone(r, KC_A4, 0, 0.6);
		double f = PeakFreq(s.L, r, 100, 2000);
		INFO("rate=%6d f0=%.2f noise=%.2e", r, f, NoiseIndex(s.L));
		CHECK(fabs(f - g_f0) < 1.0, "rate %d: f0 %.2f != %.2f", r, f, g_f0);
		CHECK(NoiseIndex(s.L) < 1e-2, "rate %d: noisy", r);
	}
}

// HQ_SINE (parabolic interpolation) must be a refinement of linear interpolation,
// i.e. nearly identical output (the old correction term was 512x too large).
static void test_fm_hq_sine_matches_linear_interpolation() {
	SetEnv({"OPM_SINE_INTERP=1"});
	Stereo lin = RenderTone(48000, KC_A4, 0, 1.0);
	SetEnv({"OPM_SINE_INTERP=1", "HQ_SINE=1"});
	Stereo hq = RenderTone(48000, KC_A4, 0, 1.0);
	double rel = RmsDiff(lin.L, hq.L) / Rms(lin.L);
	double purity = PowerAt(hq.L, 48000, g_f0) / (Rms(hq.L) * Rms(hq.L));
	INFO("rms diff linear vs HQ = %.4f%%  purity=%.5f", rel * 100, purity);
	CHECK(rel < 0.002, "HQ_SINE deviates from linear interpolation by %.3f%%", rel * 100);
	CHECK(purity > 0.98, "HQ tone not pure");
}

// HQ_SINE with feedback (self modulation) and modulators must stay sane.
static void test_fm_hq_sine_with_feedback_and_modulation() {
	SetEnv({"OPM_SINE_INTERP=1"});
	Start(48000);
	FmTone(0, KC_A4, 0, 0xC0, 7);
	Stereo lin = Render(48000, 1.0, 0.2);
	SetEnv({"OPM_SINE_INTERP=1", "HQ_SINE=1"});
	Start(48000);
	FmTone(0, KC_A4, 0, 0xC0, 7);
	Stereo hq = Render(48000, 1.0, 0.2);
	// With FB=7 only op M1 (slot 0) uses feedback; make it audible
	double c = Corr(lin.L, hq.L);
	INFO("corr(linear, HQ) = %.5f", c);
	CHECK(c > 0.99, "HQ output differs strongly from linear with feedback (corr=%.4f)", c);
}

// Octave layering must add a tone at 2x / 0.5x frequency.
static void test_fm_octave_upper_adds_octave() {
	SetEnv({"FM_OCTAVE_UPPER=1", "FM_OCTAVE_UPPER_VOL=50"});
	Stereo s = RenderTone(48000);
	double p1 = PowerAt(s.L, 48000, g_f0), p2 = PowerAt(s.L, 48000, 2 * g_f0);
	double ratio = sqrt(p2 / p1);
	INFO("amp(2f0)/amp(f0) = %.3f  noise=%.2e", ratio, NoiseIndex(s.L));
	CHECK(ratio > 0.3 && ratio < 0.7, "upper octave level %.3f (expected ~0.5)", ratio);
	CHECK(NoiseIndex(s.L) < 1e-2, "noisy");
}

static void test_fm_octave_lower_adds_octave() {
	SetEnv({"FM_OCTAVE_LOWER=1", "FM_OCTAVE_LOWER_VOL=50"});
	Stereo s = RenderTone(48000);
	double p1 = PowerAt(s.L, 48000, g_f0), p2 = PowerAt(s.L, 48000, 0.5 * g_f0);
	double ratio = sqrt(p2 / p1);
	INFO("amp(f0/2)/amp(f0) = %.3f", ratio);
	CHECK(ratio > 0.3 && ratio < 0.7, "lower octave level %.3f (expected ~0.5)", ratio);
}

// Lower octave on the lowest notes with deep vibrato (negative LFO pitch):
// must not read outside STEPTBL (was: random pitch garbage).
static void test_fm_octave_lower_low_note_with_vibrato() {
	SetEnv({"FM_OCTAVE_LOWER=1", "FM_OCTAVE_LOWER_VOL=100", "FM_OCTAVE_UPPER=1", "FM_OCTAVE_UPPER_VOL=100"});
	Start(48000);
	Opm(0x18, 0xC0);        // LFRQ
	Opm(0x19, 0x80 | 127);  // PMD max
	Opm(0x1B, 2);           // triangle
	FmTone(0, 0x00, 0);     // lowest note
	Opm(0x38, 0x70);        // PMS=7
	Stereo s = Render(48000, 1.0, 0.2);
	INFO("rms=%.0f noise=%.2e peak=%.0f", Rms(s.L), NoiseIndex(s.L), Peak(s.L));
	CHECK(NoiseIndex(s.L) < 1e-2, "garbage output (noise=%.3e)", NoiseIndex(s.L));
}

// Noise mode (channel 7 operator C2) must produce noise in both generators.
static void test_fm_noise_mode_produces_noise() {
	const char *modes[2] = {"LFSR_NOISE=0", "LFSR_NOISE=1"};
	for (int m = 0; m < 2; ++m) {
		SetEnv({modes[m]});
		Start(48000);
		Opm(0x0F, 0x80 | 0x10);   // NE=1, NFRQ
		FmTone(7, KC_A4, 0);
		Stereo s = Render(48000, 0.5, 0.2);
		int zc = 0; for (size_t i = 1; i < s.L.size(); ++i) if ((s.L[i] >= 0) != (s.L[i - 1] >= 0)) ++zc;
		INFO("%s: rms=%.0f zero-crossings/s=%.0f", modes[m], Rms(s.L), zc / 0.5);
		CHECK(Rms(s.L) > 500, "%s: noise channel silent (rms=%.0f)", modes[m], Rms(s.L));
		CHECK(zc > 500, "%s: no noise (zero crossings %d)", modes[m], zc);
	}
}

// --- Effect chain: identity / neutrality --------------------------------------

// With no effect configured, output must equal the injected signal exactly.
static void test_effects_off_is_bit_exact_passthrough() {
	SetEnv({});
	std::vector<int> w = SineWave(48000, 440, 12000, 0.3, 6000);
	Stereo s = RenderWave(48000, w);
	int bad = 0;
	for (size_t i = 0; i < s.L.size(); ++i) {
		if ((int)s.L[i] != (short)(w[i] & 0xFFFF) || (int)s.R[i] != (short)(w[i] >> 16)) ++bad;
	}
	CHECK(bad == 0, "%d samples differ from input", bad);
}

// An effect that should not touch a quiet signal (compressor below threshold)
// must not alter the mix (was: mix rebuilt from internals, phase inverted,
// WaveFunc dropped).
static void test_effect_enabled_keeps_mix_intact() {
	// Quiet signal (peak 8000 < compressor threshold 16384): COMPRESSOR=1 must be
	// a no-op, so the output must be bit-exact to the input.
	// (WaveFunc is used because OPM rendering keeps internal static timing state
	//  between runs, which shifts the waveform by a sample.)
	SetEnv({"COMPRESSOR=1"});
	std::vector<int> w = SineWave(48000, 440, 8000, 0.3, 4000);
	Stereo s = RenderWave(48000, w);
	int bad = 0;
	for (size_t i = 0; i < s.L.size(); ++i)
		if ((int)s.L[i] != (short)(w[i] & 0xFFFF) || (int)s.R[i] != (short)(w[i] >> 16)) ++bad;
	INFO("samples differing from input: %d / %zu  (rms L=%.0f)", bad, s.L.size(), Rms(s.L));
	CHECK(bad == 0, "enabling COMPRESSOR altered a quiet mix (%d samples; phase inverted / mix rebuilt?)", bad);
	CHECK(Corr(s.L, std::vector<double>(s.L)) > 0 && Rms(s.L) > 5000, "WaveFunc signal lost when effects enabled");
}

// --- Effect chain: no destruction for any option or combination ---------------

struct Combo { const char *name; std::vector<const char *> env; };

static void test_effects_no_noise_any_combination() {
	std::vector<Combo> combos = {
		{"exciter4", {"EXCITER=4"}},
		{"subbass4", {"SUB_BASS=4"}},
		{"fmharm4+warm4", {"FM_HARMONIC=4", "FM_WARMTH=4"}},
		{"chorus3", {"FM_CHORUS=3"}},
		{"width200", {"STEREO_WIDTH=200"}},
		{"reverb4", {"REVERB=4", "REVERB_DECAY=95", "REVERB_MIX=50"}},
		{"comp3", {"COMPRESSOR=3"}},
		{"crossfeed+center+haas+ER", {"CROSSFEED=100", "CENTER_WIDTH=100", "HAAS_EFFECT=100", "EARLY_REFLECTIONS=100"}},
		{"speaker preset", {"OUTPUT_RATE=96000", "STEREO_WIDTH=150", "CENTER_WIDTH=70", "EARLY_REFLECTIONS=30",
		                    "EXCITER=2", "SUB_BASS=2", "FM_HARMONIC=2", "FM_WARMTH=2", "REVERB=2", "REVERB_DECAY=70", "REVERB_MIX=20"}},
		{"headphone preset", {"OUTPUT_RATE=96000", "STEREO_WIDTH=120", "CROSSFEED=30", "CROSSFEED_DELAY=2",
		                      "HAAS_EFFECT=50", "HAAS_DELAY=5", "EXCITER=2", "SUB_BASS=1", "FM_HARMONIC=2", "FM_WARMTH=2", "REVERB=1", "REVERB_MIX=15"}},
		{"everything max", {"OUTPUT_RATE=192000", "EXCITER=4", "SUB_BASS=4", "FM_HARMONIC=4", "FM_WARMTH=4", "FM_CHORUS=3",
		                    "STEREO_WIDTH=200", "CROSSFEED=100", "CENTER_WIDTH=100", "HAAS_EFFECT=100", "EARLY_REFLECTIONS=100",
		                    "REVERB=4", "REVERB_DECAY=95", "REVERB_MIX=50", "COMPRESSOR=3", "FM_MASTER_VOLUME=200",
		                    "FM_OCTAVE_UPPER=1", "FM_OCTAVE_LOWER=1", "OPM_SINE_INTERP=1", "HQ_SINE=1"}},
	};
	SetEnv({});
	double dry_rms = Rms(RenderTone(48000, KC_A4, 0, 1.0, 4).L);
	for (auto &c : combos) {
		std::vector<const char *> env = c.env;
		// apply
		for (int i = 0; ALL_VARS[i]; ++i) unsetenv(ALL_VARS[i]);
		for (const char *kv : env) { std::string s(kv); size_t e = s.find('='); setenv(("X68SOUND_" + s.substr(0, e)).c_str(), s.substr(e + 1).c_str(), 1); }
		LoadConfigFromEnvironment();
		int rate = g_Config.output_sample_rate ? g_Config.output_sample_rate : 48000;
		// Loud material: 4 FM channels at full level (stresses overflow paths)
		Stereo s = RenderTone(rate, KC_A4, 0, 1.0, 4);
		double nL = NoiseIndex(s.L), nR = NoiseIndex(s.R);
		double jump = std::max(MaxJump(s.L), MaxJump(s.R));
		INFO("%-26s rms=%6.0f noise L=%.2e R=%.2e maxjump=%5.0f", c.name, Rms(s.L), nL, nR, jump);
		// Level may legitimately change (e.g. 50% reverb mix on a steady tone),
		// but must not collapse.
		CHECK(Rms(s.L) > 0.2 * dry_rms && Rms(s.R) > 0.2 * dry_rms, "%s: output vanished (%.0f vs dry %.0f)", c.name, Rms(s.L), dry_rms);
		CHECK(nL < 1e-2 && nR < 1e-2, "%s: destroyed/noisy output (noise L=%.3e R=%.3e)", c.name, nL, nR);
		CHECK(jump < 20000, "%s: sample discontinuity %.0f", c.name, jump);
	}
}

// Same check on a signal with silence <-> loud transitions (transients stress
// filters / envelope followers / delay buffers).
static void test_effects_no_noise_on_transients() {
	SetEnv({"EXCITER=4", "SUB_BASS=4", "FM_CHORUS=3", "REVERB=4", "COMPRESSOR=3",
	        "CROSSFEED=100", "CENTER_WIDTH=100", "HAAS_EFFECT=100", "EARLY_REFLECTIONS=100", "STEREO_WIDTH=200"});
	int rate = 48000;
	std::vector<int> w;
	for (int i = 0; i < rate * 2; ++i) {
		double t = (double)i / rate;
		double gate = (fmod(t, 0.25) < 0.125) ? 1.0 : 0.0;
		double v = gate * (0.6 * sin(2 * M_PI * 110 * t) + 0.35 * sin(2 * M_PI * 330 * t));
		w.push_back(Pack((int)(32000 * v), (int)(-32000 * v)));  // L/R out of phase: max side signal
	}
	Stereo s = RenderWave(rate, w);
	INFO("noise L=%.2e R=%.2e maxjump=%.0f", NoiseIndex(s.L), NoiseIndex(s.R), std::max(MaxJump(s.L), MaxJump(s.R)));
	CHECK(NoiseIndex(s.L) < 3e-2 && NoiseIndex(s.R) < 3e-2, "noisy output on transients");
}

// --- Individual effect behaviour -----------------------------------------------

static void test_sub_bass_adds_sub_octave() {
	SetEnv({});
	Stereo dry = RenderTone(48000, 0x2A);  // lower note (~1 octave below A4 x2)
	double f = PeakFreq(dry.L, 48000, 50, 1000);
	SetEnv({"SUB_BASS=4"});
	Stereo wet = RenderTone(48000, 0x2A);
	double d = sqrt(PowerAt(dry.L, 48000, f / 2)), w = sqrt(PowerAt(wet.L, 48000, f / 2));
	double fund = sqrt(PowerAt(wet.L, 48000, f));
	INFO("f=%.1f  sub amp dry=%.1f wet=%.1f (fund %.0f)  noise=%.2e", f, d, w, fund, NoiseIndex(wet.L));
	CHECK(w > 10 * (d + 1) && w > 0.05 * fund, "no sub-octave generated");
	CHECK(NoiseIndex(wet.L) < 1e-2, "sub bass produced noise");
}

static void test_stereo_width_scaling() {
	// Left-only signal x: with width w (Mid/Side), R' = x*(1-w)/2, L' = x*(1+w)/2
	int widths[] = {50, 150, 200};
	for (int wd : widths) {
		char e[64]; snprintf(e, sizeof(e), "STEREO_WIDTH=%d", wd);
		SetEnv({e});
		Stereo s = RenderWave(48000, SineWave(48000, 440, 10000, 0.3, 0));
		double ratio = Rms(s.R) / Rms(s.L) * (Corr(s.L, s.R) < 0 ? -1 : 1);
		double w = wd / 100.0, expect = (1 - w) / (1 + w);
		INFO("width=%d  R/L=%.3f expected=%.3f", wd, ratio, expect);
		CHECK(fabs(ratio - expect) < 0.02, "width %d: R/L %.3f, expected %.3f", wd, ratio, expect);
	}
}

static int FirstResponse(const std::vector<double> &x, double thr) {
	for (size_t i = 0; i < x.size(); ++i) if (fabs(x[i]) > thr) return (int)i;
	return -1;
}

// Haas / crossfeed delays must be in real time units at every sample rate.
static void test_stereo_enhancement_delays_scale_with_rate() {
	int rates[] = {44100, 48000, 96000, 192000};
	for (int r : rates) {
		// Haas: 5ms
		SetEnv({"HAAS_EFFECT=100", "HAAS_DELAY=5"});
		std::vector<int> w(r / 10, 0); w[100] = Pack(20000, 0);
		Stereo s = RenderWave(r, w);
		int pos = FirstResponse(s.R, 100) - 100;
		int expect = r * 5 / 1000;
		// Crossfeed: 0.2ms (0.1ms units)
		SetEnv({"CROSSFEED=100", "CROSSFEED_DELAY=2"});
		Stereo c = RenderWave(r, w);
		int cpos = FirstResponse(c.R, 100) - 100;
		int cexp = r * 2 / 10000;
		INFO("rate=%6d haas delay=%4d (expect %4d)  crossfeed delay=%3d (expect %3d)", r, pos, expect, cpos, cexp);
		CHECK(abs(pos - expect) <= 1, "rate %d: haas delay %d != %d", r, pos, expect);
		CHECK(abs(cpos - cexp) <= 1, "rate %d: crossfeed delay %d != %d", r, cpos, cexp);
	}
}

// Large values in crossfeed+center+haas must not wrap around (16-bit overflow).
static void test_stereo_enhancement_no_wraparound() {
	SetEnv({"CROSSFEED=100", "CENTER_WIDTH=100", "HAAS_EFFECT=100", "EARLY_REFLECTIONS=100"});
	// Hard-panned loud square-ish wave
	std::vector<int> w;
	for (int i = 0; i < 48000; ++i) {
		double v = sin(2 * M_PI * 200 * i / 48000.0) > 0 ? 1 : -1;
		w.push_back(Pack((int)(30000 * v), (int)(-30000 * v)));
	}
	Stereo s = RenderWave(48000, w);
	// A wrapped sample shows up as a sign flip at full scale: count sign changes
	// with large magnitude on both sides (a 200Hz square has 400/s)
	int flips = 0;
	for (size_t i = 1; i < s.L.size(); ++i)
		if (s.L[i] * s.L[i - 1] < 0 && fabs(s.L[i]) > 20000 && fabs(s.L[i - 1]) > 20000) ++flips;
	INFO("full-scale sign flips/s = %d (square wave: 400)", flips);
	CHECK(flips <= 420, "wrap-around detected (%d flips)", flips);
}

static void test_reverb_tail_decays_and_is_clean() {
	SetEnv({"REVERB=3", "REVERB_DECAY=80", "REVERB_MIX=50"});
	int rate = 48000;
	std::vector<int> w = SineWave(rate, 330, 30000, 0.5);
	w.resize(rate * 3, 0);   // then 2.5s silence
	Stereo s = RenderWave(rate, w);
	auto seg = [&](double a, double b) { return std::vector<double>(s.L.begin() + (int)(a * rate), s.L.begin() + (int)(b * rate)); };
	double t1 = Rms(seg(0.55, 0.75)), t2 = Rms(seg(1.5, 1.7)), t3 = Rms(seg(2.7, 2.9));
	INFO("tail rms: 0.05s=%.1f 1.0s=%.1f 2.2s=%.1f  clip=%d noise(main)=%.2e", t1, t2, t3, ClipCount(s.L), NoiseIndex(seg(0.1, 0.5)));
	CHECK(t1 > 100, "no reverb tail");
	CHECK(t2 < t1 && t3 <= t2, "tail does not decay");
	CHECK(t3 < 50, "reverb does not die out");
	CHECK(ClipCount(s.L) == 0, "reverb clipped");
	CHECK(NoiseIndex(seg(0.1, 0.5)) < 1e-2, "reverb noisy");
}

static void test_chorus_modulates_slowly() {
	SetEnv({"FM_CHORUS=2"});
	Stereo s = RenderTone(48000, KC_A4, 0, 2.0);
	// Chorus = dry + modulated delay => amplitude envelope beats slowly (~0.8Hz LFO)
	int blk = 48000 / 50; std::vector<double> env;
	for (size_t i = 0; i + blk <= s.L.size(); i += blk) env.push_back(Rms(std::vector<double>(s.L.begin() + i, s.L.begin() + i + blk)));
	double mn = 1e9, mx = 0; for (double e : env) { mn = std::min(mn, e); mx = std::max(mx, e); }
	// count envelope direction changes (fast LFO => many)
	int turns = 0; for (size_t i = 2; i < env.size(); ++i) if ((env[i] - env[i - 1]) * (env[i - 1] - env[i - 2]) < 0) ++turns;
	INFO("envelope min=%.0f max=%.0f turns=%d over 2s  noise=%.2e", mn, mx, turns, NoiseIndex(s.L));
	CHECK(mx > mn * 1.02, "chorus has no effect");
	CHECK(turns < 20, "chorus modulation too fast (%d turns)", turns);
	CHECK(NoiseIndex(s.L) < 1e-2, "chorus noisy");
}

// --- PCM8 ------------------------------------------------------------------------

static double g_known_issue_count = 0;
#define KNOWN_ISSUE(...) do { printf("    KNOWN ISSUE: "); printf(__VA_ARGS__); printf("\n"); g_known_issue_count++; } while (0)

static Stereo RenderPcm8Sine(int rate, double freq) {
	Start(rate);
	// 1 second, 15625Hz, 16bit big endian
	static std::vector<unsigned char> data;
	data.clear();
	for (int i = 0; i < 15625; ++i) {
		// PCM8 treats 16-bit PCM in a 12-bit range (+-2047, MAXPCMVAL) like the original
		short v = (short)(1500 * sin(2 * M_PI * freq * i / 15625.0));
		data.push_back((unsigned char)((v >> 8) & 0xFF));
		data.push_back((unsigned char)(v & 0xFF));
	}
	X68Sound_Pcm8_Out(0, data.data(), (8 << 16) | (5 << 8) | 3, (int)data.size());
	return Render(rate, 0.8, 0.1);
}

static void test_pcm8_16bit_playback_clean() {
	double purity[2];
	const char *cfgs[2] = {"LINEAR_INTERPOLATION=0", "LINEAR_INTERPOLATION=1"};
	for (int k = 0; k < 2; ++k) {
		SetEnv({cfgs[k], "VOLUME_SMOOTHING=1"});
		Stereo s = RenderPcm8Sine(48000, 500);
		double f = PeakFreq(s.L, 48000, 100, 2000);
		purity[k] = PowerAt(s.L, 48000, f) / (Rms(s.L) * Rms(s.L));
		INFO("%s: rms=%.0f f=%.1f purity=%.4f noise=%.2e", cfgs[k], Rms(s.L), f, purity[k], NoiseIndex(s.L));
		CHECK(Rms(s.L) > 300, "PCM8 silent");
		CHECK(fabs(f - 500) < 2, "PCM8 pitch %.1f", f);
		CHECK(purity[k] > 0.8, "PCM8 destroyed (purity %.3f)", purity[k]);
		CHECK(NoiseIndex(s.L) < 2e-2, "PCM8 noisy");
	}
	// Intended: interpolation should reduce zero-order-hold images.
	if (purity[1] <= purity[0] + 0.01) {
		KNOWN_ISSUE("LINEAR_INTERPOLATION has no audible effect on PCM8/ADPCM (purity %.3f -> %.3f): "
		            "the interpolated value is not used for output (15.6kHz zero-order hold). Use X68SOUND_PCM_RESAMPLE=1", purity[0], purity[1]);
	}
}


// =============================================================================
// High quality DSP (dsp.h) - X68SOUND_PCM_RESAMPLE / REVERB_ALGO / CHORUS_ALGO /
// EQ_* / LIMITER
// =============================================================================

static double DbRatio(double a, double b) { return 20 * log10(a / b); }

// 3.1 windowed-sinc resampler: removes zero-order-hold images, keeps pitch/level
static void test_hq_pcm_resampler_removes_images() {
	int rates[] = {44100, 48000, 96000, 192000};
	for (int r : rates) {
		SetEnv({});
		Stereo zoh = RenderPcm8Sine(r, 500);
		SetEnv({"PCM_RESAMPLE=1"});
		Stereo hq = RenderPcm8Sine(r, 500);
		double pz = PowerAt(zoh.L, r, 500) / (Rms(zoh.L) * Rms(zoh.L));
		double ph = PowerAt(hq.L, r, 500) / (Rms(hq.L) * Rms(hq.L));
		// strongest image of a 500Hz tone sampled at 15625Hz: 15125Hz
		double imgZ = sqrt(PowerAt(zoh.L, r, 15125) / PowerAt(zoh.L, r, 500));
		double imgH = sqrt(PowerAt(hq.L, r, 15125) / PowerAt(hq.L, r, 500));
		double lvl = DbRatio(sqrt(PowerAt(hq.L, r, 500)), sqrt(PowerAt(zoh.L, r, 500)));
		INFO("rate=%6d purity ZOH=%.4f HQ=%.5f  image@15.1k ZOH=%.1fdB HQ=%.1fdB  level diff=%.2fdB noise=%.1e",
		     r, pz, ph, 20 * log10(imgZ + 1e-12), 20 * log10(imgH + 1e-12), lvl, NoiseIndex(hq.L));
		CHECK(ph > 0.999, "rate %d: resampled tone not pure (%.4f)", r, ph);
		CHECK(20 * log10(imgH + 1e-12) < -60, "rate %d: image not suppressed", r);
		CHECK(fabs(lvl) < 1.0, "rate %d: level changed by %.2fdB", r, lvl);
		CHECK(fabs(PeakFreq(hq.L, r, 100, 2000) - 500) < 1, "rate %d: pitch", r);
	}
}

// 3.2 FDN reverb: decays (longer for larger decay), stereo, clean, calibrated level
static void test_hq_fdn_reverb() {
	int rate = 48000;
	double rt[2];
	int decays[2] = {30, 90};
	for (int k = 0; k < 2; ++k) {
		char d[64]; snprintf(d, sizeof(d), "REVERB_DECAY=%d", decays[k]);
		SetEnv({"REVERB=3", "REVERB_ALGO=1", "REVERB_MIX=50", d});
		std::vector<int> w(rate * 6, 0);
		for (int i = 0; i < rate / 10; ++i) {   // 100ms noise burst
			int v = (int)(16000.0 * ((i * 1103515245u + 12345u) >> 16 & 0x7FFF) / 32768.0 - 8000) ;
			w[i] = Pack(v, v);
		}
		Stereo s = RenderWave(rate, w);
		// energy decay curve (Schroeder) on L -> RT60 from -5..-25dB
		std::vector<double> e(s.L.size());
		double acc = 0;
		for (size_t i = s.L.size(); i-- > 0;) { acc += s.L[i] * s.L[i]; e[i] = acc; }
		int t5 = -1, t25 = -1;
		for (size_t i = rate / 10; i < e.size(); ++i) {
			double db = 10 * log10(e[i] / e[rate / 10] + 1e-30);
			if (t5 < 0 && db < -5) t5 = (int)i;
			if (t25 < 0 && db < -25) { t25 = (int)i; break; }
		}
		rt[k] = (t25 > 0 && t5 > 0) ? 3.0 * (t25 - t5) / rate : -1;
		std::vector<double> tl(s.L.begin() + rate / 5, s.L.begin() + rate), tr(s.R.begin() + rate / 5, s.R.begin() + rate);
		INFO("decay=%d  RT60~%.2fs  tail L/R corr=%.3f  clip=%d noise=%.1e", decays[k], rt[k], Corr(tl, tr), ClipCount(s.L), NoiseIndex(tl));
		CHECK(rt[k] > 0.2, "decay %d: no reverb tail", decays[k]);
		CHECK(fabs(Corr(tl, tr)) < 0.6, "decay %d: tail not decorrelated (corr %.2f)", decays[k], Corr(tl, tr));
		CHECK(ClipCount(s.L) == 0, "clipping");
		CHECK(Rms(std::vector<double>(s.L.end() - rate / 2, s.L.end())) < 5, "tail does not die out");
	}
	CHECK(rt[1] > rt[0] * 1.8, "REVERB_DECAY has no effect on RT60 (%.2f -> %.2f)", rt[0], rt[1]);
	// wet level ~ dry level (steady sine, mix=50 -> output ~ dry level)
	SetEnv({"REVERB=2", "REVERB_ALGO=1", "REVERB_MIX=50"});
	Stereo s = RenderWave(rate, SineWave(rate, 440, 10000, 2.0));
	std::vector<double> st(s.L.begin() + rate, s.L.end());
	double lv = DbRatio(Rms(st), 10000 / sqrt(2.0));
	INFO("steady level with mix=50: %.1fdB re dry", lv);
	CHECK(lv > -8 && lv < 3, "wet/dry balance off (%.1fdB)", lv);
}

// 3.2 multi-voice chorus: slow, stereo, clean
static void test_hq_multichorus() {
	SetEnv({"FM_CHORUS=2", "CHORUS_ALGO=1"});
	Stereo s = RenderTone(48000, KC_A4, 0, 2.0);
	SetEnv({});
	Stereo dry = RenderTone(48000, KC_A4, 0, 2.0);
	double c = Corr(s.L, s.R);
	int blk = 48000 / 50; std::vector<double> env;
	for (size_t i = 0; i + blk <= s.L.size(); i += blk) env.push_back(Rms(std::vector<double>(s.L.begin() + i, s.L.begin() + i + blk)));
	int turns = 0; for (size_t i = 2; i < env.size(); ++i) if ((env[i] - env[i - 1]) * (env[i - 1] - env[i - 2]) < 0) ++turns;
	double f = PeakFreq(s.L, 48000, 100, 2000);
	INFO("L/R corr=%.3f  env turns=%d  f0=%.2f  level=%.1fdB re dry  noise=%.1e", c, turns, f, DbRatio(Rms(s.L), Rms(dry.L)), NoiseIndex(s.L));
	CHECK(c < 0.99, "chorus not stereo (corr %.3f)", c);
	CHECK(turns < 30, "modulation too fast");
	CHECK(fabs(f - g_f0) < 2, "pitch shifted");
	CHECK(fabs(DbRatio(Rms(s.L), Rms(dry.L))) < 3, "level changed");
	CHECK(NoiseIndex(s.L) < 1e-3, "noisy");
}

// 3.3 parametric EQ: gains at the configured frequencies, flat elsewhere
static double EqGainDb(std::initializer_list<const char *> env, double f) {
	SetEnv(env);
	Stereo s = RenderWave(48000, SineWave(48000, f, 8000, 0.6));
	std::vector<double> st(s.L.begin() + 9600, s.L.end());
	return DbRatio(sqrt(2 * PowerAt(st, 48000, f)), 8000);   // PowerAt = A^2/2
}
static void test_hq_parametric_eq() {
	double lo = EqGainDb({"EQ_LOW_GAIN=6", "EQ_LOW_FREQ=200"}, 50);
	double lo_hi = EqGainDb({"EQ_LOW_GAIN=6", "EQ_LOW_FREQ=200"}, 5000);
	double mid = EqGainDb({"EQ_MID_GAIN=-6", "EQ_MID_FREQ=1000", "EQ_MID_Q=100"}, 1000);
	double mid_far = EqGainDb({"EQ_MID_GAIN=-6", "EQ_MID_FREQ=1000", "EQ_MID_Q=100"}, 100);
	double hi = EqGainDb({"EQ_HIGH_GAIN=6", "EQ_HIGH_FREQ=4000"}, 15000);
	double hi_lo = EqGainDb({"EQ_HIGH_GAIN=6", "EQ_HIGH_FREQ=4000"}, 200);
	INFO("low shelf +6@200Hz: 50Hz %+.2fdB, 5kHz %+.2fdB", lo, lo_hi);
	INFO("mid peak -6@1kHz:   1kHz %+.2fdB, 100Hz %+.2fdB", mid, mid_far);
	INFO("high shelf +6@4kHz: 15kHz %+.2fdB, 200Hz %+.2fdB", hi, hi_lo);
	CHECK(fabs(lo - 6) < 0.5 && fabs(lo_hi) < 0.3, "low shelf");
	CHECK(fabs(mid + 6) < 0.3 && fabs(mid_far) < 0.3, "mid peaking");
	CHECK(fabs(hi - 6) < 0.5 && fabs(hi_lo) < 0.3, "high shelf");
}

// 3.3 look-ahead limiter: no sample above the ceiling, no hard clipping,
// transparent (bit exact, delayed) below the ceiling
static void test_hq_limiter() {
	int rate = 48000;
	SetEnv({"LIMITER=1", "LIMITER_CEILING=-10", "EXCITER=4", "SUB_BASS=4", "CENTER_WIDTH=100", "CROSSFEED=100"});
	// loud material pushed beyond full scale by the effects
	std::vector<int> w;
	for (int i = 0; i < rate * 2; ++i) {
		double t = (double)i / rate, g = (fmod(t, 0.5) < 0.25) ? 1.0 : 0.3;
		double v = g * (0.7 * sin(2 * M_PI * 110 * t) + 0.3 * sin(2 * M_PI * 1760 * t));
		w.push_back(Pack((int)(32000 * v), (int)(-30000 * v)));
	}
	Stereo s = RenderWave(rate, w);
	double ceil = 32767 * pow(10, -1.0 / 20);
	double pk = std::max(Peak(s.L), Peak(s.R));
	int over = 0; for (size_t i = 0; i < s.L.size(); ++i) if (fabs(s.L[i]) > ceil + 1 || fabs(s.R[i]) > ceil + 1) ++over;
	INFO("peak=%.0f (ceiling %.0f)  samples over ceiling=%d  clipped(32767)=%d  noise=%.1e", pk, ceil, over, ClipCount(s.L) + ClipCount(s.R), NoiseIndex(s.L));
	CHECK(over == 0, "%d samples above the ceiling", over);
	CHECK(ClipCount(s.L) + ClipCount(s.R) == 0, "hard clipping remains");
	CHECK(NoiseIndex(s.L) < 2e-2, "limiter distorts");

	// transparent below the ceiling: output == input delayed by the look-ahead
	SetEnv({"LIMITER=1"});
	std::vector<int> q = SineWave(rate, 440, 10000, 0.3, 5000);
	Stereo t = RenderWave(rate, q);
	int la = (int)(rate * 1.5 / 1000), bad = 0;
	for (size_t i = la; i < t.L.size(); ++i)
		if ((int)t.L[i] != (short)(q[i - la] & 0xFFFF) || (int)t.R[i] != (short)(q[i - la] >> 16)) ++bad;
	INFO("quiet signal: %d samples differ from input delayed by %d", bad, la);
	CHECK(bad == 0, "limiter not transparent below the ceiling");
}


// 3.4 neural ADPCM enhancement (offline only): PCM8 ADPCM sine gets cleaner,
// level/pitch unchanged, and it is inactive for real-time playback
static double ThdN(const std::vector<double> &x, int rate, double f) {
	double a = 0, b = 0, w = 2 * M_PI * f / rate; size_t n = x.size();
	for (size_t i = 0; i < n; ++i) { a += x[i] * sin(w * i); b += x[i] * cos(w * i); }
	a *= 2.0 / n; b *= 2.0 / n;
	double r = 0; for (size_t i = 0; i < n; ++i) { double e = x[i] - (a * sin(w * i) + b * cos(w * i)); r += e * e; }
	return sqrt(r / n) / (sqrt(a * a + b * b) / sqrt(2.0)) * 100;
}
static std::vector<unsigned char> g_adpcm_data;
static Stereo RenderPcm8Adpcm(int rate) {
	Start(rate);
	X68Sound_Pcm8_Out(0, g_adpcm_data.data(), (8 << 16) | (4 << 8) | 3, (int)g_adpcm_data.size());
	return Render(rate, 0.8, 0.1);
}
static void test_ai_enhance_adpcm() {
	// encode a 700Hz sine (15625Hz ADPCM)
	static const int T[49] = {16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
		337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552};
	static const int D[8] = {-1,-1,-1,-1,2,4,6,8};
	int pcm = 0, sc = 0, nib = -1; g_adpcm_data.clear();
	for (int i = 0; i < 15625; ++i) {
		int v = (int)(1200 * sin(2 * M_PI * 700 * i / 15625.0));
		int st = T[sc], diff = v - pcm, code = 0;
		if (diff < 0) { code = 8; diff = -diff; }
		if (diff >= st) { code |= 4; diff -= st; }
		if (diff >= st >> 1) { code |= 2; diff -= st >> 1; }
		if (diff >= st >> 2) code |= 1;
		int d = st >> 3; if (code & 4) d += st; if (code & 2) d += st >> 1; if (code & 1) d += st >> 2;
		pcm += (code & 8) ? -d : d; if (pcm > 2047) pcm = 2047; if (pcm < -2047) pcm = -2047;
		sc += D[code & 7]; if (sc < 0) sc = 0; if (sc > 48) sc = 48;
		if (nib < 0) nib = code; else { g_adpcm_data.push_back((unsigned char)(nib | (code << 4))); nib = -1; }
	}
	SetEnv({"PCM_RESAMPLE=1"});
	Stereo off = RenderPcm8Adpcm(48000);
	SetEnv({"PCM_RESAMPLE=1", "AI_ENHANCE=1"});
	Stereo on = RenderPcm8Adpcm(48000);
	double t0 = ThdN(off.L, 48000, 700), t1 = ThdN(on.L, 48000, 700);
	double lv = 20 * log10(sqrt(PowerAt(on.L, 48000, 700) / PowerAt(off.L, 48000, 700)));
	INFO("THD+N: AI off %.3f%%  AI on %.3f%%  (%.1fdB better)  level diff %.2fdB", t0, t1, 20 * log10(t0 / t1), lv);
	CHECK(t1 < t0 * 0.8, "AI did not reduce ADPCM distortion+noise");
	CHECK(fabs(lv) < 0.5, "AI changed the level");
	CHECK(fabs(PeakFreq(on.L, 48000, 100, 2000) - 700) < 1, "AI changed the pitch");
	// real-time playback: must be inactive even if requested
	SetEnv({"PCM_RESAMPLE=1", "AI_ENHANCE=1"});
	Start(48000);
	g_OfflineRender = 0;   // what X68Sound_Start() (real-time) sets
	X68Sound_Pcm8_Out(0, g_adpcm_data.data(), (8 << 16) | (4 << 8) | 3, (int)g_adpcm_data.size());
	Stereo rt = Render(48000, 0.8, 0.1);
	g_OfflineRender = 1;
	double t2 = ThdN(rt.L, 48000, 700);
	INFO("real-time mode with AI_ENHANCE=1: THD+N %.3f%% (must equal AI off)", t2);
	CHECK(fabs(t2 - t0) < t0 * 0.01, "AI active in real-time mode");   // (render timing state may shift a sample)
}

// Everything at once (new algorithms) must stay clean
static void test_hq_all_combined_clean() {
	SetEnv({"OUTPUT_RATE=96000", "PCM_RESAMPLE=1", "REVERB=4", "REVERB_ALGO=1", "REVERB_MIX=40", "FM_CHORUS=3",
	        "CHORUS_ALGO=1", "EQ_LOW_GAIN=4", "EQ_MID_GAIN=-3", "EQ_HIGH_GAIN=3", "LIMITER=1", "EXCITER=2", "SUB_BASS=2",
	        "STEREO_WIDTH=150", "CROSSFEED=30", "HAAS_EFFECT=50", "EARLY_REFLECTIONS=30", "FM_MASTER_VOLUME=200"});
	Stereo s = RenderTone(96000, KC_A4, 0, 1.5, 4);
	INFO("rms=%.0f noise L=%.1e R=%.1e peak=%.0f clipped=%d", Rms(s.L), NoiseIndex(s.L), NoiseIndex(s.R),
	     std::max(Peak(s.L), Peak(s.R)), ClipCount(s.L) + ClipCount(s.R));
	CHECK(NoiseIndex(s.L) < 1e-2 && NoiseIndex(s.R) < 1e-2, "noisy");
	CHECK(ClipCount(s.L) + ClipCount(s.R) == 0, "clipping with limiter");
	CHECK(Rms(s.L) > 1000, "silent");
}

// =============================================================================
int main() {
	printf("============================================\n");
	printf("X68Sound2025 integration tests (real source)\n");
	printf("============================================\n");

	RUN(test_config_zero_values_are_respected);
	RUN(test_config_defaults_and_clamping);

	RUN(test_fm_default_tone_is_clean);
	RUN(test_fm_pitch_is_independent_of_sample_rate);
	RUN(test_fm_hq_sine_matches_linear_interpolation);
	RUN(test_fm_hq_sine_with_feedback_and_modulation);
	RUN(test_fm_octave_upper_adds_octave);
	RUN(test_fm_octave_lower_adds_octave);
	RUN(test_fm_octave_lower_low_note_with_vibrato);
	RUN(test_fm_noise_mode_produces_noise);

	RUN(test_effects_off_is_bit_exact_passthrough);
	RUN(test_effect_enabled_keeps_mix_intact);
	RUN(test_effects_no_noise_any_combination);
	RUN(test_effects_no_noise_on_transients);

	RUN(test_sub_bass_adds_sub_octave);
	RUN(test_stereo_width_scaling);
	RUN(test_stereo_enhancement_delays_scale_with_rate);
	RUN(test_stereo_enhancement_no_wraparound);
	RUN(test_reverb_tail_decays_and_is_clean);
	RUN(test_chorus_modulates_slowly);

	RUN(test_pcm8_16bit_playback_clean);

	RUN(test_hq_pcm_resampler_removes_images);
	RUN(test_hq_fdn_reverb);
	RUN(test_hq_multichorus);
	RUN(test_hq_parametric_eq);
	RUN(test_hq_limiter);
	RUN(test_hq_all_combined_clean);
	RUN(test_ai_enhance_adpcm);

	X68Sound_Free();
	printf("============================================\n");
	printf("Results: %d/%d tests passed, %d failed, %d known issue(s)\n", g_pass, g_run, g_fail, (int)g_known_issue_count);
	printf("============================================\n");
	return g_fail ? 1 : 0;
}

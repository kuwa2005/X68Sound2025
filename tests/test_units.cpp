// =============================================================================
// X68Sound2025 - Unit tests for internals (real source)
// =============================================================================
// Tests internal classes that cannot be reached through the public DLL API
// (e.g. the ADPCM ring buffer used by X68SOUND_ADPCM_MULTICHANNEL, which is fed
// by DMA in real use). Private members are opened for testing only.
//
// Build & run: tests/run_tests.sh
// =============================================================================

// Standard / stub headers first: the access macros below must not touch them
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <windows.h>

#define private public
#define protected public
#define class struct
#include "../X68Sound/X68Sound.cpp"
#undef class
#undef private
#undef protected

#include <vector>
#include <cmath>
#include <cstdio>

static int g_run = 0, g_pass = 0, g_fail = 0;
static bool g_cur_ok;
#define CHECK(cond, ...) do { \
	if (!(cond)) { printf("    FAIL: "); printf(__VA_ARGS__); printf("  [%s:%d]\n", __FILE__, __LINE__); g_cur_ok = false; } \
} while (0)
#define INFO(...) do { printf("    "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define RUN(fn) do { printf("[TEST] %s\n", #fn); g_run++; g_cur_ok = true; fn(); \
	if (g_cur_ok) { g_pass++; printf("    PASS\n"); } else { g_fail++; } } while (0)

static double PowerAt(const std::vector<double> &x, double rate, double f) {
	double w = 2 * M_PI * f / rate, c = 2 * cos(w), s1 = 0, s2 = 0;
	for (double v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
	return 2.0 * (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)x.size() * x.size());
}
static double Rms(const std::vector<double> &x) { double e = 0; for (double v : x) e += v * v; return sqrt(e / x.size()); }
static double NoiseIndex(const std::vector<double> &x) {
	double e = 0, d = 0;
	for (size_t i = 2; i < x.size(); ++i) { double dd = x[i] - 2 * x[i - 1] + x[i - 2]; d += dd * dd; e += x[i] * x[i]; }
	return e > 0 ? d / e : 0;
}

// -----------------------------------------------------------------------------
// ADPCM ring buffer (multichannel octave layering)
// -----------------------------------------------------------------------------
// Simulates the real call pattern: ring buffer written at the ADPCM rate
// (AdpcmRate), read once per 15625Hz tick. Each layer must output the input
// tone shifted by exactly one/two octaves, without garbage.
static Adpcm g_adpcm;

static void RingBufferCase(int adpcmRateIdx, double toneHz) {
	static const double ADPCM_HZ[5] = {3906.25, 5208.33, 7812.5, 10416.67, 15625};
	const int layers[3] = {0, 2, 3};
	const int incs[3] = {0x20000, 0x08000, 0x04000};
	const double ratio[3] = {2.0, 0.5, 0.25};
	for (int k = 0; k < 3; ++k) {
		g_adpcm.Init();
		g_adpcm.AdpcmRate = ADPCMRATEADDTBL[adpcmRateIdx];
		int rc = 0; double ph = 0;
		std::vector<double> out;
		for (int t = 0; t < 15625 * 3; ++t) {
			rc -= g_adpcm.AdpcmRate;
			while (rc < 0) {
				g_adpcm.WriteToRingBuffer((int)(20000 * sin(ph)));
				ph += 2 * M_PI * toneHz / ADPCM_HZ[adpcmRateIdx];
				rc += 15625 * 12;
			}
			int o = g_adpcm.ReadFromRingBuffer(layers[k], incs[k]);
			if (t >= 15625) out.push_back(o);
		}
		double fexp = toneHz * ratio[k];
		// Pitch-shifting by resampling re-syncs the read position periodically
		// (phase jumps), so measure purity per 0.05s block and take the median.
		std::vector<double> pur;
		for (size_t b = 0; b + 781 <= out.size(); b += 781) {
			std::vector<double> blk(out.begin() + b, out.begin() + b + 781);
			double r = Rms(blk);
			pur.push_back(r > 0 ? PowerAt(blk, 15625, fexp) / (r * r) : 0);
		}
		std::sort(pur.begin(), pur.end());
		double purity = pur[pur.size() / 2];
		INFO("ADPCM %.0fHz tone %.0fHz layer x%.2f: purity@%.1fHz=%.3f rms=%.0f noise=%.2e",
		     ADPCM_HZ[adpcmRateIdx], toneHz, ratio[k], fexp, purity, Rms(out), NoiseIndex(out));
		CHECK(Rms(out) > 5000, "layer x%.2f silent", ratio[k]);
		// Re-sync crossfades cost some purity, but the shifted pitch must dominate
		CHECK(purity > 0.8, "layer x%.2f: wrong pitch / garbage (purity %.3f)", ratio[k], purity);
		CHECK(NoiseIndex(out) < 0.05, "layer x%.2f noisy", ratio[k]);
	}
}

static void test_adpcm_ringbuffer_layers_7k8() { RingBufferCase(2, 200); }
static void test_adpcm_ringbuffer_layers_15k6() { RingBufferCase(4, 300); }
static void test_adpcm_ringbuffer_layers_3k9() { RingBufferCase(0, 150); }

// Before enough data exists the reader must output silence, not stale memory.
static void test_adpcm_ringbuffer_startup_is_silent() {
	g_adpcm.Init();
	for (int i = 0; i < Adpcm::ADPCM_RINGBUF_SIZE; ++i) g_adpcm.RingBuffer[i] = 30000;  // stale data
	g_adpcm.RingBufWritePos = 0;
	for (int l = 0; l < 4; ++l) g_adpcm.RingBufPos[l] = -1;
	int nonzero = 0;
	for (int i = 0; i < 3; ++i) {  // only 3 samples written
		g_adpcm.WriteToRingBuffer(0);
		if (g_adpcm.ReadFromRingBuffer(0, 0x20000) != 0) ++nonzero;
		if (g_adpcm.ReadFromRingBuffer(3, 0x04000) != 0) ++nonzero;
	}
	CHECK(nonzero == 0, "stale data returned at start-up (%d)", nonzero);
}

// -----------------------------------------------------------------------------
// Random number generator (noise / LFO noise waveform)
// -----------------------------------------------------------------------------
static void test_irnd_upper_bits_are_random() {
	for (int mode = 0; mode < 2; ++mode) {
		g_Config.lfsr_noise = mode;
		int plus = 0, hist[8] = {0};
		for (int i = 0; i < 40000; ++i) {
			unsigned r = irnd();
			if (((r >> 30) & 2) != 0) ++plus;       // as used by Op noise
			hist[r >> 29]++;                         // top 3 bits (LFO table index style)
		}
		int mn = 1 << 30, mx = 0; for (int h : hist) { mn = std::min(mn, h); mx = std::max(mx, h); }
		INFO("lfsr=%d: P(+1)=%.3f  top-3-bit histogram min=%d max=%d", mode, plus / 40000.0, mn, mx);
		CHECK(plus > 18000 && plus < 22000, "lfsr=%d: noise sign not balanced (%d/40000)", mode, plus);
		CHECK(mn > 3500, "lfsr=%d: upper bits not uniformly random", mode);
	}
	g_Config.lfsr_noise = 0;
}

// -----------------------------------------------------------------------------
// AudioEffects internals
// -----------------------------------------------------------------------------
static AudioEffects g_fx;

static void test_chorus_lfo_rate_is_sample_rate_independent() {
	int rates[] = {22050, 44100, 48000, 96000, 192000};
	for (int r : rates) {
		g_fx.Init(r);
		double hz = (double)g_fx.fm_chorus_lfo_inc * r / 4294967296.0;
		INFO("rate=%6d chorus LFO = %.3f Hz", r, hz);
		CHECK(fabs(hz - 0.8) < 0.01, "rate %d: LFO %.3fHz", r, hz);
	}
}

static void test_reverb_delays_scale_with_rate() {
	g_fx.Init(48000); int d48 = g_fx.ScaleDelay(1557);
	g_fx.Init(192000); int d192 = g_fx.ScaleDelay(1557);
	INFO("comb delay 48k=%d 192k=%d", d48, d192);
	CHECK(d48 == 1557 && d192 == 1557 * 4, "delay scaling");
	// Largest comb delay at 192kHz with Hall must fit the buffer
	CHECK(g_fx.ScaleDelay((1617 * 4) >> 1) + g_fx.ScaleDelay(23) < REVERB_BUFFER_SIZE, "reverb buffer too small for 192kHz");
}

// Feeding full-scale DC/square to every effect must never overflow int32
// (output stays within a sane range).
static void test_effects_bounded_with_extreme_input() {
	g_Config.exciter_level = 4; g_Config.sub_bass_level = 4; g_Config.fm_harmonic_level = 4;
	g_Config.fm_warmth_level = 4; g_Config.fm_chorus_depth = 3; g_Config.stereo_width = 200;
	g_Config.reverb_type = 4; g_Config.reverb_decay = 95; g_Config.reverb_mix = 50; g_Config.compressor_level = 3;
	g_fx.Init(48000);
	int worst = 0;
	for (int i = 0; i < 48000 * 3; ++i) {
		int v = ((i / 37) & 1) ? 65535 : -65535;   // 2x full scale square (pre-clip mix)
		int L = v, R = -v;
		g_fx.ProcessEffects(L, R, v, -v);
		worst = std::max(worst, std::max(abs(L), abs(R)));
	}
	INFO("max |output| = %d", worst);
	CHECK(worst < 400000, "effects output exploded (%d)", worst);
	g_Config.exciter_level = g_Config.sub_bass_level = g_Config.fm_harmonic_level = 0;
	g_Config.fm_warmth_level = g_Config.fm_chorus_depth = 0; g_Config.stereo_width = 100;
	g_Config.reverb_type = 0; g_Config.compressor_level = 0;
}

// Reverb impulse response must decay (stable feedback) for every type/decay.
static void test_reverb_is_stable_for_all_settings() {
	for (int type = 1; type <= 4; ++type) {
		for (int decay = 10; decay <= 95; decay += 85) {
			g_Config.reverb_type = type; g_Config.reverb_decay = decay; g_Config.reverb_mix = 50;
			g_fx.Init(48000);
			double e_early = 0, e_late = 0;
			for (int i = 0; i < 48000 * 6; ++i) {
				int L = (i == 0) ? 30000 : 0, R = L;
				g_fx.ProcessReverb(L, R);
				if (i > 48000 && i < 48000 * 2) e_early += (double)L * L;
				if (i > 48000 * 5) e_late += (double)L * L;
			}
			INFO("type %d decay %2d: energy 1-2s=%.0f  5-6s=%.0f", type, decay, e_early, e_late);
			CHECK(e_late <= e_early * 0.5 + 1, "type %d decay %d: does not decay (%.0f -> %.0f)", type, decay, e_early, e_late);
			CHECK(e_late < 100, "type %d decay %d: residual signal remains after 5s (%.0f)", type, decay, e_late);
		}
	}
	g_Config.reverb_type = 0;
}

// -----------------------------------------------------------------------------
// OPM operator: parabolic interpolation accuracy
// -----------------------------------------------------------------------------
static void test_parabolic_interpolation_error() {
	// Uses the real SINTBL built by MakeTable()
	X68Sound_StartPcm(48000, 1, 1, 5);
	double maxerr = 0;
	for (int ph = 0; ph < (SIZESINTBL << PRECISION_BITS); ph += 13) {
		int idx = (ph >> PRECISION_BITS) & (SIZESINTBL - 1), t = ph & ((1 << PRECISION_BITS) - 1);
		int s0 = SINTBL[idx], s1 = SINTBL[(idx + 1) & (SIZESINTBL - 1)], sp = SINTBL[(idx - 1) & (SIZESINTBL - 1)];
		int lin = s0 + (((s1 - s0) * t) >> PRECISION_BITS);
		int d2 = sp - (s0 << 1) + s1;
		int corr = (d2 * t * ((1 << PRECISION_BITS) - t)) >> (2 * PRECISION_BITS + 1);  // same as op.h
		double truth = sin(2 * M_PI * ph / (double)(SIZESINTBL << PRECISION_BITS)) * MAXSINVAL;
		maxerr = std::max(maxerr, fabs(lin - corr - truth));
	}
	X68Sound_Free();
	INFO("max error = %.2f / %d", maxerr, MAXSINVAL);
	CHECK(maxerr < 3.0, "parabolic interpolation error %.2f", maxerr);
}


// -----------------------------------------------------------------------------
// dsp.h building blocks
// -----------------------------------------------------------------------------
static void test_dsp_fdn_stable_all_settings() {
	int rates[] = {22050, 48000, 192000};
	int worst_bad = 0;
	for (int r : rates) for (int type = 1; type <= 4; ++type) for (int decay = 10; decay <= 95; decay += 85) {
		FdnReverb *f = new FdnReverb();
		f->Init(r, type, decay, 100);
		double e1 = 0, e2 = 0; bool finite = true;
		for (int i = 0; i < r * 8; ++i) {
			float x = (i < r / 20) ? (float)((i * 7919 % 2001) - 1000) * 20 : 0, l, rr;
			f->Process(x, x, l, rr);
			if (!(l == l) || fabsf(l) > 1e7f) finite = false;
			if (i > r && i < r * 2) e1 += l * l;
			if (i > r * 7) e2 += l * l;
		}
		if (!finite || e2 > e1 * 0.01 + 1) { worst_bad++; INFO("unstable: rate %d type %d decay %d (%.0f -> %.0f)", r, type, decay, e1, e2); }
		delete f;
	}
	CHECK(worst_bad == 0, "FDN unstable in %d settings", worst_bad);
}

static void test_dsp_limiter_never_exceeds_ceiling() {
	LookaheadLimiter *lim = new LookaheadLimiter();
	int rates[] = {44100, 192000};
	for (int r : rates) for (int c = -120; c <= 0; c += 40) {
		lim->Init(r, true, c / 10.0);
		float ceil = (float)(32767.0 * pow(10, c / 200.0));
		float worst = 0;
		unsigned seed = 1;
		for (int i = 0; i < r * 2; ++i) {
			seed = seed * 1664525u + 1013904223u;
			float burst = ((i / 997) % 3 == 0) ? 200000.0f : 3000.0f;   // sudden huge peaks
			float x = ((int)(seed >> 16) - 32768) / 32768.0f * burst, y = -x * 0.7f;
			lim->Process(x, y);
			worst = std::max(worst, std::max(fabsf(x), fabsf(y)));
		}
		INFO("rate %6d ceiling %5.1fdB: max |out| = %.1f (ceiling %.1f)", r, c / 10.0, worst, ceil);
		CHECK(worst <= ceil * 1.0001f, "limiter exceeded ceiling");
	}
	delete lim;
}

static void test_dsp_resampler_response() {
	// passband flat (<0.1dB up to 6kHz), stopband (>8kHz) suppressed
	SincResampler *rs = new SincResampler();
	double in_rate = 15625, out_rate = 48000;
	double freqs[] = {100, 1000, 4000, 6000};
	for (double f : freqs) {
		rs->Reset();
		double pos = 0, ph = 0; int n = 0; double acc = 0, cnt = 0;
		std::vector<double> y;
		for (int i = 0; i < 48000; ++i) {
			pos += in_rate / out_rate;
			while (pos >= 1) { pos -= 1; rs->Push((float)sin(ph), 0); ph += 2 * M_PI * f / in_rate; ++n; }
			float l, r; rs->Get((float)pos, l, r);
			if (i > 4800) y.push_back(l);
		}
		double a = sqrt(2 * PowerAt(y, out_rate, f));
		INFO("%5.0fHz: gain %.3fdB", f, 20 * log10(a));
		CHECK(fabs(20 * log10(a)) < 0.1, "passband ripple at %.0fHz", f);
	}
	delete rs;
}


// -----------------------------------------------------------------------------
// Neural ADPCM enhancer (ai_enhance.h) on signal types NOT used in training
// -----------------------------------------------------------------------------
static void OkiEncodeDecode(const std::vector<double> &x, std::vector<int> &dec, std::vector<int> &step) {
	static const int T[49] = {16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
		337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552};
	static const int D[8] = {-1,-1,-1,-1,2,4,6,8};
	int pcm = 0, sc = 0; dec.clear(); step.clear();
	for (double v : x) {
		int st = T[sc], diff = (int)lrint(v) - pcm, code = 0;
		if (diff < 0) { code = 8; diff = -diff; }
		if (diff >= st) { code |= 4; diff -= st; }
		if (diff >= st >> 1) { code |= 2; diff -= st >> 1; }
		if (diff >= st >> 2) code |= 1;
		int d = st >> 3; if (code & 4) d += st; if (code & 2) d += st >> 1; if (code & 1) d += st >> 2;
		pcm += (code & 8) ? -d : d; if (pcm > 2047) pcm = 2047; if (pcm < -2047) pcm = -2047;
		step.push_back(st); dec.push_back(pcm & ~3);
		sc += D[code & 7]; if (sc < 0) sc = 0; if (sc > 48) sc = 48;
	}
}
static void test_ai_enhancer_unseen_signals() {
	const char *names[4] = {"linear chirp 50Hz-6kHz", "square wave 220Hz", "decaying sine bursts", "sawtooth 330Hz (unseen)"};
	double fs = 15625;
	for (int k = 0; k < 4; ++k) {
		std::vector<double> x;
		for (int i = 0; i < (int)fs * 2; ++i) {
			double t = i / fs, v;
			if (k == 0) v = 1500 * sin(2 * M_PI * (50 * t + (6000 - 50) / 4.0 * t * t));
			else if (k == 1) { double ph = fmod(220 * t, 1.0); v = (ph < 0.5 ? 900 : -900); }
			else if (k == 2) v = 1800 * exp(-fmod(t, 0.25) * 12) * sin(2 * M_PI * 660 * t);
			else v = 1600 * (2 * fmod(330 * t, 1.0) - 1);
			x.push_back(v);
		}
		std::vector<int> dec, step; OkiEncodeDecode(x, dec, step);
		NeuralAdpcmEnhancer *ai = new NeuralAdpcmEnhancer();
		double es = 0, ed = 0, ee = 0;
		for (size_t i = 0; i < x.size(); ++i) {
			float y = ai->Process((float)dec[i], (float)step[i], 1.0f);
			if (i >= AI_FUTURE + 100) {
				double c = x[i - AI_FUTURE];
				es += c * c; ed += (c - dec[i - AI_FUTURE]) * (c - dec[i - AI_FUTURE]); ee += (c - y) * (c - y);
			}
		}
		double sd = 10 * log10(es / ed), se = 10 * log10(es / ee);
		INFO("%-24s SNR decoded %.2fdB -> AI %.2fdB (%+.2fdB)", names[k], sd, se, se - sd);
		CHECK(se > sd - 0.1, "%s: AI made it worse", names[k]);
		delete ai;
	}
	// strength 0 must be an exact (delayed) bypass
	NeuralAdpcmEnhancer *ai = new NeuralAdpcmEnhancer();
	int bad = 0; std::vector<int> hist;
	for (int i = 0; i < 2000; ++i) {
		int d = (int)(1000 * sin(i * 0.1)) & ~3; hist.push_back(d);
		float y = ai->Process((float)d, 64.0f, 0.0f);
		if (i >= AI_FUTURE && fabsf(y - hist[i - AI_FUTURE]) > 0) ++bad;
	}
	CHECK(bad == 0, "strength 0 is not a bypass (%d)", bad);
	delete ai;
}

int main() {
	printf("============================================\n");
	printf("X68Sound2025 unit tests (real source)\n");
	printf("============================================\n");
	RUN(test_adpcm_ringbuffer_layers_7k8);
	RUN(test_adpcm_ringbuffer_layers_15k6);
	RUN(test_adpcm_ringbuffer_layers_3k9);
	RUN(test_adpcm_ringbuffer_startup_is_silent);
	RUN(test_irnd_upper_bits_are_random);
	RUN(test_chorus_lfo_rate_is_sample_rate_independent);
	RUN(test_reverb_delays_scale_with_rate);
	RUN(test_effects_bounded_with_extreme_input);
	RUN(test_reverb_is_stable_for_all_settings);
	RUN(test_parabolic_interpolation_error);
	RUN(test_dsp_fdn_stable_all_settings);
	RUN(test_dsp_limiter_never_exceeds_ceiling);
	RUN(test_dsp_resampler_response);
	RUN(test_ai_enhancer_unseen_signals);
	printf("============================================\n");
	printf("Results: %d/%d tests passed, %d failed\n", g_pass, g_run, g_fail);
	printf("============================================\n");
	return g_fail ? 1 : 0;
}

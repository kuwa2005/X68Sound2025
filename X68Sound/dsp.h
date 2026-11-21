// ============================================================================
// X68Sound2025 - High quality DSP building blocks (floating point)
// ============================================================================
//  SincResampler   : windowed-sinc (Kaiser) polyphase interpolator used to
//                    convert the 15.625kHz ADPCM/PCM8 stream to the output rate
//                    (X68SOUND_PCM_RESAMPLE=1)
//  FdnReverb       : 8-line feedback delay network reverb with Householder
//                    mixing, modulated delays, frequency dependent decay and
//                    pre-delay (X68SOUND_REVERB_ALGO=1)
//  MultiChorus     : 3-voice stereo chorus, Hermite interpolation
//                    (X68SOUND_CHORUS_ALGO=1)
//  ParametricEQ    : low shelf / mid peaking / high shelf (RBJ biquads)
//                    (X68SOUND_EQ_*)
//  LookaheadLimiter: true peak-safe look-ahead limiter (X68SOUND_LIMITER=1)
// All processors are disabled by default (original sound is unchanged).
// ============================================================================
#ifndef X68_DSP_H
#define X68_DSP_H

#include <math.h>
#include <string.h>

#ifndef X68_PI
#define X68_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Windowed-sinc polyphase interpolator
// ---------------------------------------------------------------------------
class SincResampler {
public:
	enum { TAPS = 32, HALF = TAPS / 2, PHASES = 256, HIST = 64, HMASK = HIST - 1 };

	SincResampler() { BuildTable(0.45, 8.0); Reset(); }

	void Reset() {
		memset(hist, 0, sizeof(hist));
		pos = 0;
	}

	// Push one input sample (input rate) for channel 0/1
	void Push(float l, float r) {
		pos = (pos + 1) & HMASK;
		hist[0][pos] = l;
		hist[1][pos] = r;
	}

	// Interpolated value at (latest input sample - HALF + frac), frac in [0,1).
	// Latency: HALF input samples (~1ms at 15.625kHz).
	void Get(float frac, float &l, float &r) const {
		if (frac < 0) frac = 0;
		if (frac >= 1) frac = 0.99999f;
		float fp = frac * PHASES;
		int ph = (int)fp;
		float t = fp - ph;
		const float *k0 = table[ph];
		const float *k1 = table[ph + 1];
		float al = 0, ar = 0;
		// sample index for tap j: pos - (TAPS-1) + j  (oldest .. newest)
		int base = pos - (TAPS - 1);
		for (int j = 0; j < TAPS; ++j) {
			float c = k0[j] + (k1[j] - k0[j]) * t;
			int idx = (base + j) & HMASK;
			al += c * hist[0][idx];
			ar += c * hist[1][idx];
		}
		l = al; r = ar;
	}

private:
	float table[PHASES + 1][TAPS];
	float hist[2][HIST];
	int pos;

	static double BesselI0(double x) {
		double sum = 1, term = 1, y = x * x / 4;
		for (int k = 1; k < 50; ++k) { term *= y / (k * k); sum += term; if (term < 1e-12 * sum) break; }
		return sum;
	}

	// cutoff: fraction of the INPUT sample rate (0.5 = Nyquist)
	void BuildTable(double cutoff, double beta) {
		double i0b = BesselI0(beta);
		for (int p = 0; p <= PHASES; ++p) {
			double frac = (double)p / PHASES;
			double sum = 0;
			double c[TAPS];
			for (int j = 0; j < TAPS; ++j) {
				// distance (in input samples) between output position and tap j
				// output position = newest - HALF + frac ; tap j = newest - (TAPS-1) + j
				double x = (double)(j - (TAPS - 1)) + HALF - frac;    // = tap - out
				double s = (fabs(x) < 1e-9) ? 2 * cutoff : sin(2 * X68_PI * cutoff * x) / (X68_PI * x);
				double wn = x / (HALF + 0.5);
				double w = (fabs(wn) >= 1) ? 0 : BesselI0(beta * sqrt(1 - wn * wn)) / i0b;
				c[j] = s * w;
				sum += c[j];
			}
			for (int j = 0; j < TAPS; ++j) table[p][j] = (float)(c[j] / sum);   // unity DC gain
		}
	}
};

// ---------------------------------------------------------------------------
// RBJ biquad
// ---------------------------------------------------------------------------
struct Biquad {
	float b0, b1, b2, a1, a2;
	float z1[2], z2[2];
	Biquad() { SetIdentity(); }
	void SetIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; Reset(); }
	void Reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
	inline float Run(int ch, float x) {
		float y = b0 * x + z1[ch];
		z1[ch] = b1 * x - a1 * y + z2[ch];
		z2[ch] = b2 * x - a2 * y;
		return y;
	}
	void Set(double B0, double B1, double B2, double A0, double A1, double A2) {
		b0 = (float)(B0 / A0); b1 = (float)(B1 / A0); b2 = (float)(B2 / A0);
		a1 = (float)(A1 / A0); a2 = (float)(A2 / A0);
	}
	void LowShelf(double fs, double f, double gainDb, double S = 1.0) {
		double A = pow(10, gainDb / 40), w = 2 * X68_PI * f / fs, cw = cos(w), sw = sin(w);
		double alpha = sw / 2 * sqrt((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * sqrt(A) * alpha;
		Set(A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sa),
		    (A + 1) + (A - 1) * cw + sa, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa);
	}
	void HighShelf(double fs, double f, double gainDb, double S = 1.0) {
		double A = pow(10, gainDb / 40), w = 2 * X68_PI * f / fs, cw = cos(w), sw = sin(w);
		double alpha = sw / 2 * sqrt((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * sqrt(A) * alpha;
		Set(A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sa),
		    (A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa);
	}
	void Peaking(double fs, double f, double gainDb, double Q) {
		double A = pow(10, gainDb / 40), w = 2 * X68_PI * f / fs, alpha = sin(w) / (2 * Q), cw = cos(w);
		Set(1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
	}
	void LowPass1(double fs, double f) {   // used as one-pole-like smoother (Q=0.707)
		double w = 2 * X68_PI * f / fs, cw = cos(w), alpha = sin(w) / (2 * 0.7071);
		Set((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
	}
};

// ---------------------------------------------------------------------------
// 3-band parametric EQ
// ---------------------------------------------------------------------------
class ParametricEQ {
public:
	bool enabled;
	ParametricEQ() : enabled(false) {}
	void Init(double fs, int lowGain, int lowFreq, int midGain, int midFreq, int midQx100, int highGain, int highFreq) {
		enabled = (lowGain != 0 || midGain != 0 || highGain != 0);
		double nyq = fs * 0.45;
		low.LowShelf(fs, Clampd(lowFreq, 20, nyq), lowGain);
		mid.Peaking(fs, Clampd(midFreq, 20, nyq), midGain, Clampd(midQx100 / 100.0, 0.1, 10));
		high.HighShelf(fs, Clampd(highFreq, 20, nyq), highGain);
		if (lowGain == 0) low.SetIdentity();
		if (midGain == 0) mid.SetIdentity();
		if (highGain == 0) high.SetIdentity();
	}
	inline void Process(float &l, float &r) {
		l = high.Run(0, mid.Run(0, low.Run(0, l)));
		r = high.Run(1, mid.Run(1, low.Run(1, r)));
	}
private:
	Biquad low, mid, high;
	static double Clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
};

// ---------------------------------------------------------------------------
// Look-ahead limiter (stereo linked)
// ---------------------------------------------------------------------------
class LookaheadLimiter {
public:
	enum { MAXLA = 2048 };
	bool enabled;
	LookaheadLimiter() : enabled(false), la(1), ceiling(32000), rel(0.999f), att(0.5f), gain(1) { Reset(); }
	void Init(double fs, bool en, double ceilingDb, double lookaheadMs = 1.5, double releaseMs = 80) {
		enabled = en;
		ceiling = (float)(32767.0 * pow(10, ceilingDb / 20));
		la = (int)(fs * lookaheadMs / 1000);
		if (la < 1) la = 1;
		if (la > MAXLA - 1) la = MAXLA - 1;
		rel = (float)exp(-1.0 / (fs * releaseMs / 1000));
		att = (float)exp(-6.0 / la);    // ~99.75% of the way within the look-ahead window
		Reset();
	}
	void Reset() {
		memset(bufL, 0, sizeof(bufL)); memset(bufR, 0, sizeof(bufR));
		w = 0; gain = 1; qh = qt = 0; cnt = 0;
	}
	// returns delayed, limited samples (latency = la samples)
	inline void Process(float &l, float &r) {
		float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
		float g = (pk > ceiling) ? ceiling / pk : 1.0f;     // gain required for this sample
		bufL[w] = l; bufR[w] = r;
		// sliding-window minimum of required gain over the look-ahead window (monotonic deque)
		while (qt != qh && dqv[(qt - 1) & (MAXLA - 1)] >= g) qt = (qt - 1) & (MAXLA - 1);
		dqv[qt] = g; dqi[qt] = cnt; qt = (qt + 1) & (MAXLA - 1);
		while (dqi[qh] + la < cnt) qh = (qh + 1) & (MAXLA - 1);
		float target = dqv[qh];
		// smooth attack: the window minimum is known la samples before the peak, so
		// the gain glides down within the look-ahead time (no clicks); exponential release
		if (target < gain) gain = target + (gain - target) * att;
		else gain = target + (gain - target) * rel;
		int rd = (w - la) & (MAXLA - 1);
		l = bufL[rd] * gain; r = bufR[rd] * gain;
		// final safety (numerical)
		if (l > ceiling) l = ceiling; if (l < -ceiling) l = -ceiling;
		if (r > ceiling) r = ceiling; if (r < -ceiling) r = -ceiling;
		w = (w + 1) & (MAXLA - 1); cnt++;
	}
private:
	float bufL[MAXLA], bufR[MAXLA];
	float dqv[MAXLA]; long long dqi[MAXLA];
	int qh, qt; long long cnt = 0;
	int w, la;
	float ceiling, rel, att, gain;
};

// ---------------------------------------------------------------------------
// FDN reverb (8 lines, Householder feedback)
// ---------------------------------------------------------------------------
class FdnReverb {
public:
	enum { N = 8, MAXD = 32768, PREMAX = 32768 };
	FdnReverb() { fs = 48000; Init(48000, 2, 70, 0); }

	// type 1..4 (small..hall), decay 10..95, predelayMs 0..100
	void Init(double sampleRate, int type, int decay, int predelayMs) {
		fs = sampleRate;
		static const double baseMs[N] = {29.7, 37.1, 41.1, 43.7, 47.9, 53.3, 59.9, 67.7};  // mutually prime-ish
		double size = 0.5 + 0.35 * (type - 1);               // 0.5, 0.85, 1.2, 1.55
		// RT60: decay 10..95 -> 0.3s .. 4.0s, scaled by size
		double rt60 = (0.3 + (decay - 10) / 85.0 * 3.7) * (0.6 + 0.4 * size);
		for (int i = 0; i < N; ++i) {
			double ms = baseMs[i] * size;
			int d = (int)(ms * fs / 1000);
			if (d > MAXD - 64) d = MAXD - 64;
			if (d < 8) d = 8;
			delay[i] = d;
			// per-line gain for RT60: g = 10^(-3 * d / (fs * rt60))
			g[i] = (float)pow(10, -3.0 * d / (fs * rt60));
			// high frequencies decay ~2x faster (one-pole damping in the loop)
			double rt60hi = rt60 * 0.5;
			double ghi = pow(10, -3.0 * d / (fs * rt60hi));
			// one pole: y = (1-a) x + a y ; HF gain ratio ~ (1-a)/(1+a) at Nyquist
			double ratio = ghi / g[i];
			damp[i] = (float)((1 - ratio) / (1 + ratio));
			if (damp[i] < 0) damp[i] = 0;
			if (damp[i] > 0.9f) damp[i] = 0.9f;
			lp[i] = 0;
			modPhase[i] = (float)(i * 0.785);
			modInc[i] = (float)(2 * X68_PI * (0.1 + 0.07 * i) / fs);
		}
		modDepth = (float)(0.0012 * fs * size);   // ~1ms * size (gentle, avoids pitch wobble)
		pre = (int)(predelayMs * fs / 1000);
		if (pre > PREMAX - 1) pre = PREMAX - 1;
		// input/output tone: band-limit the wet signal
		inLp.LowPass1(fs, 9000 > fs * 0.45 ? fs * 0.45 : 9000);
		inHp.Set(1, -1, 0, 1, -0.995, 0);   // DC blocker
		Reset();
	}
	void Reset() {
		memset(buf, 0, sizeof(buf)); memset(preBuf, 0, sizeof(preBuf));
		for (int i = 0; i < N; ++i) lp[i] = 0;
		w = 0; pw = 0; inLp.Reset(); inHp.Reset();
	}
	// returns wet stereo for the given dry stereo input
	inline void Process(float inL, float inR, float &outL, float &outR) {
		float x = (inL + inR) * 0.5f;
		x = inHp.Run(0, inLp.Run(0, x));
		preBuf[pw] = x;
		float xin = preBuf[(pw - pre) & (PREMAX - 1)];
		pw = (pw + 1) & (PREMAX - 1);

		float o[N], s = 0;
		for (int i = 0; i < N; ++i) {
			// modulated read (linear interpolation)
			modPhase[i] += modInc[i];
			if (modPhase[i] > 2 * X68_PI) modPhase[i] -= (float)(2 * X68_PI);
			float d = delay[i] - modDepth * (0.5f + 0.5f * sinf(modPhase[i]));
			int di = (int)d; float f = d - di;
			float a = buf[i][(w - di) & (MAXD - 1)], b = buf[i][(w - di - 1) & (MAXD - 1)];
			float y = a + (b - a) * f;
			// frequency dependent decay
			lp[i] = y * (1 - damp[i]) + lp[i] * damp[i];
			o[i] = lp[i] * g[i];
			s += o[i];
		}
		// Householder: v_i = o_i - (2/N) * sum
		float h = s * (2.0f / N);
		for (int i = 0; i < N; ++i) {
			float sign = (i & 1) ? -1.0f : 1.0f;
			buf[i][w] = o[i] - h + xin * 0.35f * sign;
		}
		w = (w + 1) & (MAXD - 1);
		// decorrelated stereo taps
		outL = (o[0] - o[2] + o[4] - o[6]) * 0.6f + (o[1] + o[5]) * 0.3f;
		outR = (o[1] - o[3] + o[5] - o[7]) * 0.6f + (o[2] + o[6]) * 0.3f;
	}
private:
	double fs;
	float buf[N][MAXD];
	float preBuf[PREMAX];
	int delay[N];
	float g[N], damp[N], lp[N];
	float modPhase[N], modInc[N], modDepth;
	int w, pw, pre;
	Biquad inLp, inHp;
};

// ---------------------------------------------------------------------------
// 3-voice stereo chorus (Hermite interpolation)
// ---------------------------------------------------------------------------
class MultiChorus {
public:
	enum { MAXD = 8192, V = 3 };
	MultiChorus() { Init(48000, 2); }
	void Init(double sampleRate, int depthLevel) {
		fs = sampleRate;
		static const double rates[V] = {0.53, 0.79, 1.13};
		double depthMs = 0.9 + 0.6 * depthLevel;                // 1.5 / 2.1 / 2.7 ms
		for (int v = 0; v < V; ++v) {
			phase[v] = (float)(v * 2 * X68_PI / V);
			inc[v] = (float)(2 * X68_PI * rates[v] / fs);
			base[v] = (float)((7.0 + 3.0 * v) * fs / 1000);     // 7 / 10 / 13 ms
		}
		depth = (float)(depthMs * fs / 1000);
		mix = 0.35f + 0.1f * depthLevel;
		memset(bufL, 0, sizeof(bufL)); memset(bufR, 0, sizeof(bufR)); w = 0;
	}
	inline void Process(float &l, float &r) {
		bufL[w] = l; bufR[w] = r;
		float wl = 0, wr = 0;
		for (int v = 0; v < V; ++v) {
			phase[v] += inc[v];
			if (phase[v] > 2 * X68_PI) phase[v] -= (float)(2 * X68_PI);
			float m = sinf(phase[v]);
			float dl = base[v] + depth * (0.5f + 0.5f * m);
			float dr = base[v] + depth * (0.5f - 0.5f * m);   // opposite modulation on R
			wl += Read(bufL, dl) * ((v == 1) ? 0.5f : (v == 0 ? 1.0f : 0.25f));
			wr += Read(bufR, dr) * ((v == 1) ? 0.5f : (v == 2 ? 1.0f : 0.25f));
		}
		w = (w + 1) & (MAXD - 1);
		float g = mix / 1.75f;
		l = l * (1 - mix * 0.5f) + wl * g;
		r = r * (1 - mix * 0.5f) + wr * g;
	}
private:
	double fs;
	float bufL[MAXD], bufR[MAXD];
	float phase[V], inc[V], base[V], depth, mix;
	int w;
	inline float Read(const float *b, float d) const {
		int i = (int)d; float t = d - i;
		float xm1 = b[(w - i + 1) & (MAXD - 1)], x0 = b[(w - i) & (MAXD - 1)];
		float x1 = b[(w - i - 1) & (MAXD - 1)], x2 = b[(w - i - 2) & (MAXD - 1)];
		// 4-point Hermite
		float c0 = x0, c1 = 0.5f * (x1 - xm1);
		float c2 = xm1 - 2.5f * x0 + 2 * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * t + c2) * t + c1) * t + c0;
	}
};

#endif // X68_DSP_H

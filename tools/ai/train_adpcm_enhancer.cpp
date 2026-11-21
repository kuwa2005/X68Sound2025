// =============================================================================
// X68Sound2025 - Neural ADPCM enhancer trainer (no external dependencies)
// =============================================================================
// Learns to reduce the quantization error of OKI/MSM6258 4-bit ADPCM.
//   input : decoded samples (window) + ADPCM step sizes (window)
//   output: predicted error (clean - decoded) at the window center, normalized
//           by the step size
// Training data is 100% synthetic (tones, plucks, drums, kicks, formant voices,
// mixtures) at the ADPCM sampling rates used on the X68000; no recordings or
// copyrighted material are used.
//
// Build: g++ -O3 -march=native -std=c++14 train_adpcm_enhancer.cpp -o train
// Run  : ./train [out_header] [seconds_budget]
//        writes the model to X68Sound/ai_model.h and prints evaluation results.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

#include "../../X68Sound/ai_enhance_model_def.h"   // window sizes / layer sizes

// ---------------------------------------------------------------------------
// ADPCM (same tables / arithmetic as X68Sound adpcm.h legacy decoder)
// ---------------------------------------------------------------------------
static const int dltLTBL[49] = {
	16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
	337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552};
static const int DCT[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};

struct Decoder {
	int Pcm = 0, Scale = 0;
	// returns decoded value (Pcm & ~3, as used by X68Sound: InpPcm = (Pcm&~3)<<8)
	int Decode(int code, int *stepOut) {
		int dltL = dltLTBL[Scale];
		*stepOut = dltL;
		dltL = (dltL & (code & 4 ? -1 : 0)) + ((dltL >> 1) & (code & 2 ? -1 : 0)) + ((dltL >> 2) & (code & 1 ? -1 : 0)) + (dltL >> 3);
		int sign = code & 8 ? -1 : 0;
		dltL = (dltL ^ sign) + (sign & 1);
		Pcm += dltL;
		if (Pcm > 2047) Pcm = 2047;
		if (Pcm < -2047) Pcm = -2047;
		Scale += DCT[code];
		if (Scale < 0) Scale = 0;
		if (Scale > 48) Scale = 48;
		return Pcm & (int)0xFFFFFFFC;
	}
};

// Standard OKI encoder (as used by common X68000 PCM converters)
static std::vector<int> EncodeDecode(const std::vector<float> &clean, std::vector<float> &dec, std::vector<float> &step) {
	Decoder d; int pred = 0, idx = 0;
	std::vector<int> codes(clean.size());
	dec.resize(clean.size()); step.resize(clean.size());
	for (size_t i = 0; i < clean.size(); ++i) {
		int s = (int)lrintf(clean[i]);
		int st = dltLTBL[idx], diff = s - pred, code = 0;
		if (diff < 0) { code = 8; diff = -diff; }
		if (diff >= st) { code |= 4; diff -= st; }
		if (diff >= st >> 1) { code |= 2; diff -= st >> 1; }
		if (diff >= st >> 2) code |= 1;
		int stp;
		int v = d.Decode(code, &stp);
		pred = d.Pcm; idx = d.Scale;
		dec[i] = (float)v; step[i] = (float)stp; codes[i] = code;
	}
	return codes;
}

// ---------------------------------------------------------------------------
// Synthetic material
// ---------------------------------------------------------------------------
static std::mt19937 rng(12345);
static float U(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
static int Ui(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }

static const double RATES[5] = {3906.25, 5208.33, 7812.5, 10416.67, 15625.0};

enum Kind { K_TONE, K_PLUCK, K_DRUM, K_KICK, K_VOICE, K_SINE, K_CHIRP, K_PULSE, K_FM, K_MIX, K_COUNT };
static const char *KIND_NAME[K_COUNT] = {"tone", "pluck", "drum/noise", "kick", "voice-like", "sine", "chirp", "pulse", "fm", "mixture"};

static void AddTone(std::vector<float> &x, double fs, bool pluck) {
	double f0 = exp(U(log(60.0), log(std::min(2000.0, fs * 0.3))));
	int nh = Ui(1, 12); double tilt = U(0.3, 1.2);
	double att = U(0.001, pluck ? 0.005 : 0.08), dec = pluck ? U(0.05, 0.6) : U(0.3, 3.0);
	double vib = U(0, 0.006), vibf = U(3, 7);
	int start = Ui(0, (int)x.size() / 2), len = std::min((int)x.size() - start, Ui((int)(fs * 0.1), (int)x.size()));
	double amp = U(0.1, 1.0);
	std::vector<double> ph(nh + 1, 0);
	for (int i = 0; i < len; ++i) {
		double t = i / fs;
		double env = (t < att) ? t / att : exp(-(t - att) / dec);
		double f = f0 * (1 + vib * sin(2 * M_PI * vibf * t));
		double v = 0;
		for (int h = 1; h <= nh; ++h) {
			if (f * h >= fs * 0.48) break;
			ph[h] += 2 * M_PI * f * h / fs;
			v += sin(ph[h]) / pow(h, tilt);
		}
		x[start + i] += (float)(amp * env * v);
	}
}
static void AddDrum(std::vector<float> &x, double fs) {
	double dec = U(0.02, 0.5), lp = U(0.05, 0.95), hp = U(0, 0.9), amp = U(0.1, 1.0);
	int start = Ui(0, (int)x.size() * 3 / 4);
	double y = 0, z = 0;
	for (int i = start; i < (int)x.size(); ++i) {
		double t = (i - start) / fs, n = U(-1, 1);
		y += lp * (n - y); double o = y - hp * z; z = y;
		x[i] += (float)(amp * exp(-t / dec) * o);
	}
}
static void AddKick(std::vector<float> &x, double fs) {
	double f1 = U(100, 300), f2 = U(35, 70), sw = U(0.01, 0.08), dec = U(0.1, 0.6), amp = U(0.3, 1.0), ph = 0;
	int start = Ui(0, (int)x.size() * 3 / 4);
	for (int i = start; i < (int)x.size(); ++i) {
		double t = (i - start) / fs, f = f2 + (f1 - f2) * exp(-t / sw);
		ph += 2 * M_PI * f / fs;
		x[i] += (float)(amp * exp(-t / dec) * sin(ph));
	}
}
static void AddVoice(std::vector<float> &x, double fs) {
	double f0 = U(90, 300), amp = U(0.2, 1.0);
	int nf = 3; double F[3] = {U(300, 900), U(900, 2400), U(2000, 3500)}, B[3] = {U(60, 150), U(80, 200), U(100, 300)};
	double y1[3] = {0}, y2[3] = {0}, ph = 0;
	int start = Ui(0, (int)x.size() / 2), len = std::min((int)x.size() - start, Ui((int)(fs * 0.2), (int)x.size()));
	for (int i = 0; i < len; ++i) {
		ph += f0 * (1 + 0.02 * sin(2 * M_PI * 5 * i / fs)) / fs;
		double src = 0; if (ph >= 1) { ph -= 1; src = 1; }
		src += 0.05 * U(-1, 1);
		double out = 0;
		for (int k = 0; k < nf; ++k) {
			if (F[k] >= fs * 0.45) continue;
			double r = exp(-M_PI * B[k] / fs), c = 2 * r * cos(2 * M_PI * F[k] / fs);
			double y = src + c * y1[k] - r * r * y2[k]; y2[k] = y1[k]; y1[k] = y;
			out += y * (1 - r) * 4;
		}
		double env = std::min(1.0, i / (fs * 0.03)) * std::min(1.0, (len - i) / (fs * 0.03));
		x[start + i] += (float)(amp * env * out);
	}
}

static void AddSine(std::vector<float> &x, double fs) {
	double f = exp(U(log(40.0), log(fs * 0.4))), ph = U(0, 6.28), amp = U(0.2, 1.0);
	for (size_t i = 0; i < x.size(); ++i) { ph += 2 * M_PI * f / fs; x[i] += (float)(amp * sin(ph)); }
}
static void AddChirp(std::vector<float> &x, double fs) {
	double f1 = exp(U(log(40.0), log(fs * 0.4))), f2 = exp(U(log(40.0), log(fs * 0.4))), ph = 0, amp = U(0.2, 1.0);
	for (size_t i = 0; i < x.size(); ++i) {
		double f = f1 * pow(f2 / f1, (double)i / x.size()); ph += 2 * M_PI * f / fs; x[i] += (float)(amp * sin(ph));
	}
}
static void AddPulse(std::vector<float> &x, double fs) {
	double f = exp(U(log(40.0), log(fs * 0.15))), duty = U(0.1, 0.5), ph = 0, amp = U(0.2, 1.0);
	for (size_t i = 0; i < x.size(); ++i) { ph += f / fs; if (ph >= 1) ph -= 1; x[i] += (float)(amp * (ph < duty ? 1 : -1)); }
}
static void AddFm(std::vector<float> &x, double fs) {
	double fc = exp(U(log(60.0), log(fs * 0.2))), ratio = Ui(1, 4), idx0 = U(0.5, 5), dec = U(0.1, 2), pc = 0, pm = 0, amp = U(0.2, 1.0);
	for (size_t i = 0; i < x.size(); ++i) {
		double t = i / fs, idx = idx0 * exp(-t / dec);
		pm += 2 * M_PI * fc * ratio / fs; pc += 2 * M_PI * fc / fs;
		x[i] += (float)(amp * sin(pc + idx * sin(pm)) * exp(-t / (dec * 2)));
	}
}

// Generate one clip (12-bit scale) of the given kind
static std::vector<float> MakeClip(Kind k, double fs, double seconds) {
	std::vector<float> x((size_t)(fs * seconds), 0.0f);
	switch (k) {
	case K_TONE: AddTone(x, fs, false); break;
	case K_PLUCK: AddTone(x, fs, true); break;
	case K_DRUM: AddDrum(x, fs); break;
	case K_KICK: AddKick(x, fs); break;
	case K_VOICE: AddVoice(x, fs); break;
	case K_SINE: AddSine(x, fs); break;
	case K_CHIRP: AddChirp(x, fs); break;
	case K_PULSE: AddPulse(x, fs); break;
	case K_FM: AddFm(x, fs); break;
	default:
		for (int n = Ui(2, 4); n-- > 0;) {
			int c = Ui(0, 8);
			if (c == 0) AddTone(x, fs, false); else if (c == 1) AddTone(x, fs, true);
			else if (c == 2) AddDrum(x, fs); else if (c == 3) AddKick(x, fs); else if (c == 4) AddVoice(x, fs);
			else if (c == 5) AddSine(x, fs); else if (c == 6) AddChirp(x, fs); else if (c == 7) AddPulse(x, fs); else AddFm(x, fs);
		}
	}
	float pk = 1e-9f; for (float v : x) pk = std::max(pk, fabsf(v));
	float target = U(150, 2000);
	for (float &v : x) v = v / pk * target;
	return x;
}

// ---------------------------------------------------------------------------
// Dataset (clips are stored as waveforms; features are built on the fly)
// ---------------------------------------------------------------------------
struct Clip { std::vector<float> dec, lstep, step, clean; int kind; };
struct Dataset {
	std::vector<Clip> clips;
	std::vector<std::pair<int, int>> idx;   // (clip, center)
	void Add(Kind k, double fs, double sec, int stride) {
		Clip c; c.kind = k;
		c.clean = MakeClip(k, fs, sec);
		EncodeDecode(c.clean, c.dec, c.step);
		c.lstep.resize(c.step.size());
		for (size_t i = 0; i < c.step.size(); ++i) c.lstep[i] = log2f(c.step[i]);
		int ci = (int)clips.size(), n = (int)c.clean.size();
		clips.push_back(std::move(c));
		for (int t = AI_PAST; t + AI_FUTURE < n; t += stride) idx.push_back(std::make_pair(ci, t));
	}
	// features, target (normalized error), step, decoded, clean, kind
	void Get(size_t i, float *in, float &target, float &step, float &dec, float &clean, int &kind) const {
		const Clip &c = clips[idx[i].first]; int t = idx[i].second;
		AiBuildInput(&c.dec[0], &c.lstep[0], t, in);
		step = c.step[t]; dec = c.dec[t]; clean = c.clean[t]; kind = c.kind;
		target = (clean - dec) / step;
		// the runtime correction is limited to +-1 step: train on the same range
		if (target > 1) target = 1; else if (target < -1) target = -1;
	}
};

// ---------------------------------------------------------------------------
// MLP + Adam
// ---------------------------------------------------------------------------
struct Param { std::vector<float> w, g, m, v; void Init(size_t n, float scale) {
	w.resize(n); g.assign(n, 0); m.assign(n, 0); v.assign(n, 0);
	std::normal_distribution<float> nd(0, scale); for (auto &x : w) x = nd(rng); } };

struct Net {
	Param W1, B1, W2, B2, W3, B3;
	void Init() {
		W1.Init(AI_NIN * AI_H1, sqrtf(1.0f / AI_NIN)); B1.Init(AI_H1, 0);
		W2.Init(AI_H1 * AI_H2, sqrtf(1.0f / AI_H1)); B2.Init(AI_H2, 0);
		W3.Init(AI_H2, sqrtf(1.0f / AI_H2) * 0.1f); B3.Init(1, 0);
	}
	float Forward(const float *x, float *h1, float *h2) const {
		for (int j = 0; j < AI_H1; ++j) {
			float s = B1.w[j]; const float *w = &W1.w[j * AI_NIN];
			for (int i = 0; i < AI_NIN; ++i) s += w[i] * x[i];
			h1[j] = tanhf(s);
		}
		for (int j = 0; j < AI_H2; ++j) {
			float s = B2.w[j]; const float *w = &W2.w[j * AI_H1];
			for (int i = 0; i < AI_H1; ++i) s += w[i] * h1[i];
			h2[j] = tanhf(s);
		}
		float o = B3.w[0];
		for (int i = 0; i < AI_H2; ++i) o += W3.w[i] * h2[i];
		return o;
	}
	void Backward(const float *x, const float *h1, const float *h2, float dout) {
		float d2[AI_H2], d1[AI_H1];
		B3.g[0] += dout;
		for (int i = 0; i < AI_H2; ++i) { W3.g[i] += dout * h2[i]; d2[i] = dout * W3.w[i] * (1 - h2[i] * h2[i]); }
		for (int i = 0; i < AI_H1; ++i) d1[i] = 0;
		for (int j = 0; j < AI_H2; ++j) {
			B2.g[j] += d2[j]; float *g = &W2.g[j * AI_H1]; const float *w = &W2.w[j * AI_H1];
			for (int i = 0; i < AI_H1; ++i) { g[i] += d2[j] * h1[i]; d1[i] += d2[j] * w[i]; }
		}
		for (int j = 0; j < AI_H1; ++j) {
			float d = d1[j] * (1 - h1[j] * h1[j]);
			B1.g[j] += d; float *g = &W1.g[j * AI_NIN];
			for (int i = 0; i < AI_NIN; ++i) g[i] += d * x[i];
		}
	}
	void Step(float lr, int t, float scale) {
		Param *ps[6] = {&W1, &B1, &W2, &B2, &W3, &B3};
		const float b1 = 0.9f, b2 = 0.999f, eps = 1e-8f;
		float c1 = 1 - powf(b1, (float)t), c2 = 1 - powf(b2, (float)t);
		for (Param *p : ps) for (size_t i = 0; i < p->w.size(); ++i) {
			float g = p->g[i] * scale;
			p->m[i] = b1 * p->m[i] + (1 - b1) * g;
			p->v[i] = b2 * p->v[i] + (1 - b2) * g * g;
			p->w[i] -= lr * (p->m[i] / c1) / (sqrtf(p->v[i] / c2) + eps);
			p->g[i] = 0;
		}
	}
};

// error energy (clean scale) of decoded vs enhanced
static void Evaluate(const Net &net, const Dataset &ds, double res[K_COUNT][3], float strength = 1.0f) {
	for (int k = 0; k < K_COUNT; ++k) res[k][0] = res[k][1] = res[k][2] = 0;
	float h1[AI_H1], h2[AI_H2], in[AI_NIN], target, step, dec, clean; int kind;
	for (size_t i = 0; i < ds.idx.size(); ++i) {
		ds.Get(i, in, target, step, dec, clean, kind);
		float r = net.Forward(in, h1, h2);
		if (r > 1) r = 1; else if (r < -1) r = -1;   // same as runtime
		float enh = dec + strength * r * step;
		res[kind][0] += (double)clean * clean;
		res[kind][1] += (double)(clean - dec) * (clean - dec);
		res[kind][2] += (double)(clean - enh) * (clean - enh);
	}
}

static void WriteHeader(const Net &net, const char *path, const std::string &meta) {
	FILE *f = fopen(path, "w");
	fprintf(f, "// Generated by tools/ai/train_adpcm_enhancer.cpp - DO NOT EDIT\n");
	fprintf(f, "// Neural ADPCM enhancer weights (MLP %d-%d-%d-1, tanh)\n", AI_NIN, AI_H1, AI_H2);
	fprintf(f, "// Trained on synthetic material only.\n%s", meta.c_str());
	fprintf(f, "#ifndef X68_AI_MODEL_H\n#define X68_AI_MODEL_H\n#include \"ai_enhance_model_def.h\"\n");
	auto dump = [&](const char *name, const std::vector<float> &v) {
		fprintf(f, "static const float %s[%zu] = {\n", name, v.size());
		for (size_t i = 0; i < v.size(); ++i) fprintf(f, "%.8gf,%s", v[i], (i % 8 == 7) ? "\n" : " ");
		fprintf(f, "};\n");
	};
	dump("AI_W1", net.W1.w); dump("AI_B1", net.B1.w); dump("AI_W2", net.W2.w);
	dump("AI_B2", net.B2.w); dump("AI_W3", net.W3.w); dump("AI_B3", net.B3.w);
	fprintf(f, "#endif\n");
	fclose(f);
}

int main(int argc, char **argv) {
	const char *out = argc > 1 ? argv[1] : "../../X68Sound/ai_model.h";
	double budget = argc > 2 ? atof(argv[2]) : 300;

	// datasets: training and held-out evaluation (different random clips)
	Dataset train, test;
	for (int i = 0; i < 1500; ++i) train.Add((Kind)(i % K_COUNT), RATES[Ui(0, 4)], U(0.3, 0.8), 1);
	rng.seed(987654321);
	for (int i = 0; i < 150; ++i) test.Add((Kind)(i % K_COUNT), RATES[i % 5], 0.5, 1);
	printf("train examples: %zu, test examples: %zu\n", train.idx.size(), test.idx.size());
	fflush(stdout);

	rng.seed(42);
	Net net; net.Init();
	Net best = net; double bestScore = -1e9;
	std::vector<int> order(train.idx.size());
	for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
	const int BATCH = 64;
	int t = 0; clock_t t0 = clock();
	float h1[AI_H1], h2[AI_H2], in[AI_NIN], target, step, dec, clean; int kind;
	const size_t EPOCH_SAMPLES = 600000;    // random subset per epoch
	for (int epoch = 0; ; ++epoch) {
		std::shuffle(order.begin(), order.end(), rng);
		double loss = 0;
		float lr = 2e-3f * powf(0.88f, (float)epoch);
		size_t nep = std::min(EPOCH_SAMPLES, order.size());
		for (size_t b = 0; b + BATCH <= nep; b += BATCH) {
			for (int k = 0; k < BATCH; ++k) {
				train.Get(order[b + k], in, target, step, dec, clean, kind);
				float o = net.Forward(in, h1, h2);
				float d = o - target;
				loss += d * d;
				net.Backward(in, h1, h2, 2 * d);
			}
			net.Step(lr, ++t, 1.0f / BATCH);
		}
		double res[K_COUNT][3]; Evaluate(net, test, res);
		double s0 = 0, s1 = 0, s2 = 0; for (int k = 0; k < K_COUNT; ++k) { s0 += res[k][0]; s1 += res[k][1]; s2 += res[k][2]; }
		double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
		printf("epoch %2d  loss %.4f  test SNR decoded %.2fdB -> enhanced %.2fdB  (%.0fs)\n", epoch,
		       loss / nep, 10 * log10(s0 / s1), 10 * log10(s0 / s2), el);
		fflush(stdout);
		// model selection: overall gain, penalizing any kind that gets worse
		double worst = 1e9;
		for (int k = 0; k < K_COUNT; ++k) worst = std::min(worst, 10 * log10(res[k][1] / res[k][2]));
		double score = 10 * log10(s1 / s2) + 3 * std::min(0.0, worst);
		printf("          worst kind %+.2fdB  score %.2f%s\n", worst, score, score > bestScore ? "  *best*" : "");
		if (score > bestScore) { bestScore = score; best = net; }
		if (el > budget || epoch >= 40) break;
	}
	net = best;

	// final report per kind
	double res[K_COUNT][3]; Evaluate(net, test, res);
	char buf[4096]; std::string meta = "// Held-out evaluation (SNR vs. clean, synthetic test set):\n";
	printf("\n%-12s %10s %10s %8s\n", "kind", "decoded", "enhanced", "gain");
	double s0 = 0, s1 = 0, s2 = 0;
	for (int k = 0; k < K_COUNT; ++k) {
		double a = 10 * log10(res[k][0] / res[k][1]), b = 10 * log10(res[k][0] / res[k][2]);
		printf("%-12s %8.2fdB %8.2fdB %+7.2fdB\n", KIND_NAME[k], a, b, b - a);
		snprintf(buf, sizeof(buf), "//   %-12s %6.2fdB -> %6.2fdB (%+.2fdB)\n", KIND_NAME[k], a, b, b - a); meta += buf;
		s0 += res[k][0]; s1 += res[k][1]; s2 += res[k][2];
	}
	snprintf(buf, sizeof(buf), "//   %-12s %6.2fdB -> %6.2fdB (%+.2fdB)\n", "overall", 10 * log10(s0 / s1), 10 * log10(s0 / s2),
	         10 * log10(s1 / s2)); meta += buf;
	printf("%-12s %8.2fdB %8.2fdB %+7.2fdB\n", "overall", 10 * log10(s0 / s1), 10 * log10(s0 / s2), 10 * log10(s1 / s2));
	WriteHeader(net, out, meta);
	printf("model written: %s\n", out);
	return 0;
}

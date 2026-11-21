// ============================================================================
// Audio Effects Processor for X68Sound2025
// ============================================================================
// High-quality audio effects using integer arithmetic
// All effects can be toggled via environment variables
//
// Fixes for "random-noise like" destruction when combining options:
//  - ProcessEffects() no longer rebuilds the mix from FM/ADPCM internals
//    (that dropped soft clipping / WaveFunc and inverted the phase).
//    The already mixed signal is used, and FM effects are applied as a delta.
//  - Harmonic exciter: int32 overflow in x*|x| removed (input/HPF clamped, int64).
//  - Sub bass: the old "phase accumulator" flipped the sign of the signal at an
//    amplitude-dependent, chaotic rate (= ring modulation with noise).
//    Replaced by a classic zero-crossing octave divider.
//  - FM warmth/harmonic: int32 overflow for |x|>46340 removed, DC removed.
//  - FM chorus: LFO was ~23Hz with integer delay steps (zipper/warble).
//    Now ~0.8Hz, sample-rate scaled, fractional delay.
//  - Stereo imager: duplicated crossfeed (subtracting, while the Opm side adds)
//    removed; width scaling fixed to percent (100 = unchanged).
//  - Reverb: comb buffers were hard-clipped inside the feedback loop
//    (input not attenuated, gain up to 3.9x) -> harsh distortion.
//    Input is now attenuated, damping added, delays scaled by sample rate,
//    allpass is a proper Schroeder allpass.
// ============================================================================

#ifndef AUDIOEFFECTS_H
#define AUDIOEFFECTS_H

#include "global.h"
#include "dsp.h"

class AudioEffects {
private:
	// Reverb state variables
	int reverb_comb_buffer_L[REVERB_COMB_COUNT][REVERB_BUFFER_SIZE];
	int reverb_comb_buffer_R[REVERB_COMB_COUNT][REVERB_BUFFER_SIZE];
	int reverb_allpass_buffer_L[REVERB_ALLPASS_COUNT][REVERB_BUFFER_SIZE];
	int reverb_allpass_buffer_R[REVERB_ALLPASS_COUNT][REVERB_BUFFER_SIZE];
	int reverb_comb_damp_L[REVERB_COMB_COUNT];
	int reverb_comb_damp_R[REVERB_COMB_COUNT];
	int reverb_pos;

	// Reverb delay times (in samples, optimized for 48kHz)
	static const int comb_delays[REVERB_COMB_COUNT];
	static const int allpass_delays[REVERB_ALLPASS_COUNT];

	// FM Chorus state
	int fm_chorus_buffer_L[CHORUS_BUFFER_SIZE];
	int fm_chorus_buffer_R[CHORUS_BUFFER_SIZE];
	int fm_chorus_pos;
	unsigned int fm_chorus_lfo_phase;
	unsigned int fm_chorus_lfo_inc;

	// FM harmonic DC blocker state
	int fm_dc_L, fm_dc_R;

	// High-pass filter states for exciter
	int exciter_highpass_L_prev;
	int exciter_highpass_R_prev;
	int exciter_highpass_L_out;
	int exciter_highpass_R_out;

	// Sub-harmonic (octave divider) state
	int bass_lp1_L, bass_lp1_R;      // input low-pass (<<8 precision)
	int bass_lp2_L, bass_lp2_R;      // output low-pass (<<8 precision)
	int bass_coef;                   // low-pass coefficient (0-65536)
	int subharm_pos_L, subharm_pos_R;    // zero-crossing hysteresis state
	int subharm_flip_L, subharm_flip_R;  // octave divider flip-flop (+1/-1)

	// Compressor state (envelope followers)
	int comp_mid_env_L, comp_mid_env_R;

	// Sample rate (for delay calculations)
	int sample_rate;

	// [HQ] high quality algorithms (dsp.h)
	FdnReverb fdn;            // X68SOUND_REVERB_ALGO=1
	MultiChorus mchorus;      // X68SOUND_CHORUS_ALGO=1
	float fdn_wet_gain;

public:
	AudioEffects();
	void Init(int samprate);
	void Reset();

	// Returns non-zero when at least one effect of this class is enabled
	static int IsAnyEnabled();

	// Main processing chain
	// out_L/out_R: already mixed output (all sources), modified in place
	// fm_L/fm_R  : FM contribution contained in out_L/out_R (same sign/scale)
	void ProcessEffects(int &out_L, int &out_R, int fm_L, int fm_R);

	// Individual effect processors
	void ProcessHarmonicExciter(int &L, int &R);
	void ProcessSubBass(int &L, int &R);
	void ProcessFMHarmonic(int &fm_L, int &fm_R);
	void ProcessFMChorus(int &fm_L, int &fm_R);
	void ProcessStereoImager(int &L, int &R);
	void ProcessReverb(int &L, int &R);
	void ProcessCompressor(int &L, int &R);

	// Helper functions
	inline int SoftClip(int input, int threshold);
	inline int FastAbs(int x) { return (x < 0) ? -x : x; }
	inline int Clamp(int x, int lim) { return (x > lim) ? lim : ((x < -lim) ? -lim : x); }
	inline int ScaleDelay(int samples48k) {
		int d = (int)(((int64_t)samples48k * sample_rate) / 48000);
		if (d < 1) d = 1;
		return d;
	}
};

// Reverb delay times for Freeverb algorithm (in samples @ 48kHz)
const int AudioEffects::comb_delays[REVERB_COMB_COUNT] = {
	1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116
};

const int AudioEffects::allpass_delays[REVERB_ALLPASS_COUNT] = {
	225, 556, 441, 341
};

AudioEffects::AudioEffects() {
	sample_rate = 48000;
	Reset();
}

void AudioEffects::Init(int samprate) {
	if (samprate < 8000) samprate = 8000;
	if (samprate > 192000) samprate = 192000;
	sample_rate = samprate;
	Reset();
	// [HQ] algorithms are (re)configured from the current settings
	int rtype = g_Config.reverb_type ? g_Config.reverb_type : 2;
	fdn.Init(sample_rate, rtype, g_Config.reverb_decay, g_Config.reverb_predelay);
	mchorus.Init(sample_rate, g_Config.fm_chorus_depth ? g_Config.fm_chorus_depth : 2);
	fdn_wet_gain = 1.4f;	// calibrated: steady-state wet level ~= dry level
}

int AudioEffects::IsAnyEnabled() {
	return g_Config.exciter_level != 0
		|| g_Config.sub_bass_level != 0
		|| g_Config.fm_harmonic_level != 0
		|| g_Config.fm_warmth_level != 0
		|| g_Config.fm_chorus_depth != 0
		|| g_Config.stereo_width != 100
		|| g_Config.reverb_type != 0
		|| g_Config.compressor_level != 0;
}

void AudioEffects::Reset() {
	// Clear reverb buffers
	for (int i = 0; i < REVERB_COMB_COUNT; i++) {
		for (int j = 0; j < REVERB_BUFFER_SIZE; j++) {
			reverb_comb_buffer_L[i][j] = 0;
			reverb_comb_buffer_R[i][j] = 0;
		}
		reverb_comb_damp_L[i] = reverb_comb_damp_R[i] = 0;
	}
	for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
		for (int j = 0; j < REVERB_BUFFER_SIZE; j++) {
			reverb_allpass_buffer_L[i][j] = 0;
			reverb_allpass_buffer_R[i][j] = 0;
		}
	}
	reverb_pos = 0;

	// Clear FM chorus buffers
	for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
		fm_chorus_buffer_L[i] = 0;
		fm_chorus_buffer_R[i] = 0;
	}
	fm_chorus_pos = 0;
	fm_chorus_lfo_phase = 0;
	// ~0.8Hz LFO: inc = 2^32 * 0.8 / fs
	fm_chorus_lfo_inc = (unsigned int)(4294967296.0 * 0.8 / sample_rate);

	fm_dc_L = fm_dc_R = 0;

	// Clear filter states
	exciter_highpass_L_prev = exciter_highpass_R_prev = 0;
	exciter_highpass_L_out = exciter_highpass_R_out = 0;

	// Sub bass: one-pole low-pass at ~150Hz: coef = exp(-2*pi*fc/fs) * 65536
	bass_coef = (int)(exp(-2.0 * 3.14159265358979 * 150.0 / sample_rate) * 65536.0);
	bass_lp1_L = bass_lp1_R = 0;
	bass_lp2_L = bass_lp2_R = 0;
	subharm_pos_L = subharm_pos_R = 0;
	subharm_flip_L = subharm_flip_R = 1;

	// Clear compressor state
	comp_mid_env_L = comp_mid_env_R = 0;
}

// Soft clipping function for harmonic generation
inline int AudioEffects::SoftClip(int input, int threshold) {
	if (FastAbs(input) <= threshold) {
		return input;
	}
	int sign = (input < 0) ? -1 : 1;
	int abs_val = FastAbs(input);
	int excess = abs_val - threshold;
	// Soft knee compression: y = threshold + excess/2
	return sign * (threshold + (excess >> 1));
}

// ============================================================================
// Harmonic Exciter - High frequency enhancement
// ============================================================================
void AudioEffects::ProcessHarmonicExciter(int &L, int &R) {
	if (g_Config.exciter_level == 0) return;

	int inL = Clamp(L, 32767);
	int inR = Clamp(R, 32767);

	// Extract high frequency content with high-pass filter
	int high_L = (int)((((int64_t)inL << 8) - ((int64_t)exciter_highpass_L_prev << 8) + 250 * (int64_t)exciter_highpass_L_out) >> 8);
	int high_R = (int)((((int64_t)inR << 8) - ((int64_t)exciter_highpass_R_prev << 8) + 250 * (int64_t)exciter_highpass_R_out) >> 8);
	high_L = Clamp(high_L, 32767);
	high_R = Clamp(high_R, 32767);

	exciter_highpass_L_prev = inL;
	exciter_highpass_R_prev = inR;
	exciter_highpass_L_out = high_L;
	exciter_highpass_R_out = high_R;

	// Generate harmonics (|high| <= 32767, so products fit in int32)
	int harmonic2_L = (high_L * FastAbs(high_L)) >> 16;   // <= 16383
	int harmonic2_R = (high_R * FastAbs(high_R)) >> 16;

	int harmonic3_L = (harmonic2_L * high_L) >> 16;       // <= 8191
	int harmonic3_R = (harmonic2_R * high_R) >> 16;

	// Mix harmonics based on intensity level (0-4)
	int intensity = g_Config.exciter_level << 4;  // 0, 16, 32, 48, 64
	L += ((harmonic2_L + (harmonic3_L >> 1)) * intensity) >> 8;
	R += ((harmonic2_R + (harmonic3_R >> 1)) * intensity) >> 8;
}

// ============================================================================
// Sub-Harmonic Bass - Low frequency enhancement (octave divider)
// ============================================================================
void AudioEffects::ProcessSubBass(int &L, int &R) {
	if (g_Config.sub_bass_level == 0) return;

	const int HYST = 64 << 8;  // zero-crossing hysteresis
	int a = bass_coef;
	int b = 65536 - a;

	// --- Left ---
	{
		int x = Clamp(L, 32767) << 8;
		bass_lp1_L = (int)(((int64_t)bass_lp1_L * a + (int64_t)x * b) >> 16);
		// Toggle the flip-flop on every positive-going zero crossing -> 1/2 frequency
		if (!subharm_pos_L && bass_lp1_L > HYST) {
			subharm_pos_L = 1;
			subharm_flip_L = -subharm_flip_L;
		} else if (subharm_pos_L && bass_lp1_L < -HYST) {
			subharm_pos_L = 0;
		}
		int sub = (subharm_flip_L > 0) ? bass_lp1_L : -bass_lp1_L;
		bass_lp2_L = (int)(((int64_t)bass_lp2_L * a + (int64_t)sub * b) >> 16);
	}
	// --- Right ---
	{
		int x = Clamp(R, 32767) << 8;
		bass_lp1_R = (int)(((int64_t)bass_lp1_R * a + (int64_t)x * b) >> 16);
		if (!subharm_pos_R && bass_lp1_R > HYST) {
			subharm_pos_R = 1;
			subharm_flip_R = -subharm_flip_R;
		} else if (subharm_pos_R && bass_lp1_R < -HYST) {
			subharm_pos_R = 0;
		}
		int sub = (subharm_flip_R > 0) ? bass_lp1_R : -bass_lp1_R;
		bass_lp2_R = (int)(((int64_t)bass_lp2_R * a + (int64_t)sub * b) >> 16);
	}

	// Mix sub-harmonic based on intensity level (0-4)
	int intensity = g_Config.sub_bass_level << 3;  // 0, 8, 16, 24, 32
	L += ((bass_lp2_L >> 8) * intensity) >> 6;
	R += ((bass_lp2_R >> 8) * intensity) >> 6;
}

// ============================================================================
// FM Harmonic Enhancer - Makes FM sound more analog
// ============================================================================
void AudioEffects::ProcessFMHarmonic(int &fm_L, int &fm_R) {
	if (g_Config.fm_harmonic_level == 0 && g_Config.fm_warmth_level == 0) return;

	int xL = Clamp(fm_L, 32767);
	int xR = Clamp(fm_R, 32767);

	// Generate even harmonics for warmth (x^2 >= 0, |x|<=32767 -> fits in int32)
	int harmonic2_L = (xL * xL) >> 16;   // 0..16383
	int harmonic2_R = (xR * xR) >> 16;

	// Add 2nd harmonic (warmth) with DC removal (x^2 has a large DC component)
	if (g_Config.fm_warmth_level > 0) {
		int warmth = g_Config.fm_warmth_level << 3;  // 0, 8, 16, 24, 32
		int wL = (harmonic2_L * warmth) >> 8;
		int wR = (harmonic2_R * warmth) >> 8;
		// One-pole DC tracker (<<8 precision, ~ few Hz)
		fm_dc_L += ((wL << 8) - fm_dc_L) >> 10;
		fm_dc_R += ((wR << 8) - fm_dc_R) >> 10;
		fm_L += wL - (fm_dc_L >> 8);
		fm_R += wR - (fm_dc_R >> 8);
	}

	// Add 3rd harmonic (richness)
	if (g_Config.fm_harmonic_level > 0) {
		int harmonic3_L = (xL * harmonic2_L) >> 16;   // |.| <= 8191
		int harmonic3_R = (xR * harmonic2_R) >> 16;

		int richness = g_Config.fm_harmonic_level << 3;
		fm_L += (harmonic3_L * richness) >> 9;
		fm_R += (harmonic3_R * richness) >> 9;
	}

	// Soft clipping to prevent overflow
	fm_L = SoftClip(fm_L, 30000);
	fm_R = SoftClip(fm_R, 30000);
}

// ============================================================================
// FM Chorus - Adds depth and width to FM sound
// ============================================================================
void AudioEffects::ProcessFMChorus(int &fm_L, int &fm_R) {
	if (g_Config.fm_chorus_depth == 0) return;

	if (g_Config.chorus_algo == 1) {
		// [HQ] 3-voice stereo chorus (float, Hermite interpolation)
		float l = (float)fm_L, r = (float)fm_R;
		mchorus.Process(l, r);
		fm_L = (int)lrintf(l); fm_R = (int)lrintf(r);
		return;
	}

	// Store input in delay buffer
	fm_chorus_buffer_L[fm_chorus_pos & CHORUS_BUFFER_MASK] = fm_L;
	fm_chorus_buffer_R[fm_chorus_pos & CHORUS_BUFFER_MASK] = fm_R;

	// Slow LFO (~0.8Hz, independent of sample rate)
	fm_chorus_lfo_phase += fm_chorus_lfo_inc;
	int lfo = SINTBL[(fm_chorus_lfo_phase >> (32 - SIZESINTBL_BITS)) & (SIZESINTBL-1)];  // +-MAXSINVAL
	// R channel uses LFO shifted by 90 degrees for stereo width
	int lfoR = SINTBL[((fm_chorus_lfo_phase >> (32 - SIZESINTBL_BITS)) + (SIZESINTBL/4)) & (SIZESINTBL-1)];

	// Delay time (16.16 fixed, scaled to sample rate)
	// base: 80/110/140 samples @48kHz, modulation: +-24/48/72 samples @48kHz
	int depth = g_Config.fm_chorus_depth;
	int64_t base = (int64_t)ScaleDelay(50 + depth * 30) << 16;
	int64_t mod_amp = (int64_t)ScaleDelay(24 * depth) << 16;
	int64_t dL = base + (mod_amp * lfo) / MAXSINVAL;
	int64_t dR = base + (mod_amp * lfoR) / MAXSINVAL;
	if (dL < (2 << 16)) dL = 2 << 16;
	if (dR < (2 << 16)) dR = 2 << 16;
	if (dL > ((int64_t)(CHORUS_BUFFER_SIZE - 2) << 16)) dL = (int64_t)(CHORUS_BUFFER_SIZE - 2) << 16;
	if (dR > ((int64_t)(CHORUS_BUFFER_SIZE - 2) << 16)) dR = (int64_t)(CHORUS_BUFFER_SIZE - 2) << 16;

	// Fractional delay read with linear interpolation
	int iL = (int)(dL >> 16), fL = (int)(dL & 0xFFFF);
	int iR = (int)(dR >> 16), fR = (int)(dR & 0xFFFF);
	int a0 = fm_chorus_buffer_L[(fm_chorus_pos - iL) & CHORUS_BUFFER_MASK];
	int a1 = fm_chorus_buffer_L[(fm_chorus_pos - iL - 1) & CHORUS_BUFFER_MASK];
	int b0 = fm_chorus_buffer_R[(fm_chorus_pos - iR) & CHORUS_BUFFER_MASK];
	int b1 = fm_chorus_buffer_R[(fm_chorus_pos - iR - 1) & CHORUS_BUFFER_MASK];
	int delayed_L = a0 + (int)(((int64_t)(a1 - a0) * fL) >> 16);
	int delayed_R = b0 + (int)(((int64_t)(b1 - b0) * fR) >> 16);

	// Mix dry and wet (50/50 for chorus effect)
	fm_L = (fm_L + delayed_L) >> 1;
	fm_R = (fm_R + delayed_R) >> 1;

	fm_chorus_pos++;
}

// ============================================================================
// Stereo Imager - stereo width (Mid/Side)
// Crossfeed / Haas are handled by Opm::ApplyStereoEnhancement().
// ============================================================================
void AudioEffects::ProcessStereoImager(int &L, int &R) {
	if (g_Config.stereo_width == 100) return;

	int mid = (L + R) >> 1;
	int side = (L - R) >> 1;

	// Adjust width (100=normal, 150=wide, 200=ultra wide)
	side = (side * g_Config.stereo_width) / 100;

	L = mid + side;
	R = mid - side;
}

// ============================================================================
// Ambience Reverb - Freeverb style (comb + allpass)
// ============================================================================
void AudioEffects::ProcessReverb(int &L, int &R) {
	if (g_Config.reverb_type == 0) return;

	if (g_Config.reverb_algo == 1) {
		// [HQ] FDN reverb (float). dry = 100-mix %, wet = mix %
		float wl, wr;
		fdn.Process((float)L, (float)R, wl, wr);
		float dry = (100 - g_Config.reverb_mix) / 100.0f, wet = g_Config.reverb_mix / 100.0f * fdn_wet_gain;
		L = (int)lrintf(L * dry + wl * wet);
		R = (int)lrintf(R * dry + wr * wet);
		return;
	}

	const int BUF_LIMIT = 1 << 20;   // safety only (never reached in normal use)
	const int DAMP = 77;             // damping 0.3 (/256)
	int decay = g_Config.reverb_decay;  // 10-95 (/128 -> max 0.74)

	// Attenuate comb input: comb gain is up to 1/(1-0.74) = 3.9x
	int inL = Clamp(L, 32767) >> 2;
	int inR = Clamp(R, 32767) >> 2;

	// Comb filters (parallel)
	int comb_out_L = 0, comb_out_R = 0;

	for (int i = 0; i < REVERB_COMB_COUNT; i++) {
		// Scale delay time based on reverb type (1=Small, 2=Medium, 3=Large, 4=Hall) and sample rate
		int delay = ScaleDelay((comb_delays[i] * g_Config.reverb_type) >> 1);
		int delayR = delay + ScaleDelay(23);   // stereo spread
		if (delayR > REVERB_BUFFER_SIZE - 1) delayR = REVERB_BUFFER_SIZE - 1;
		if (delay > REVERB_BUFFER_SIZE - 1) delay = REVERB_BUFFER_SIZE - 1;

		int yL = reverb_comb_buffer_L[i][(reverb_pos - delay) & REVERB_BUFFER_MASK];
		int yR = reverb_comb_buffer_R[i][(reverb_pos - delayR) & REVERB_BUFFER_MASK];

		// Damping low-pass inside the feedback loop
		// Note: use division (rounds toward zero), not >> (rounds toward -inf),
		// otherwise the feedback loop gets stuck at -1 (limit cycle / DC).
		reverb_comb_damp_L[i] = (yL * (256 - DAMP) + reverb_comb_damp_L[i] * DAMP) / 256;
		reverb_comb_damp_R[i] = (yR * (256 - DAMP) + reverb_comb_damp_R[i] * DAMP) / 256;

		int new_val_L = inL + (reverb_comb_damp_L[i] * decay) / 128;
		int new_val_R = inR + (reverb_comb_damp_R[i] * decay) / 128;
		reverb_comb_buffer_L[i][reverb_pos & REVERB_BUFFER_MASK] = Clamp(new_val_L, BUF_LIMIT);
		reverb_comb_buffer_R[i][reverb_pos & REVERB_BUFFER_MASK] = Clamp(new_val_R, BUF_LIMIT);

		comb_out_L += yL;
		comb_out_R += yR;
	}

	// Average comb outputs
	int ap_L = comb_out_L >> 3;
	int ap_R = comb_out_R >> 3;

	// All-pass filters (series) for diffusion: Schroeder allpass, g = 0.5
	for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
		int d = ScaleDelay(allpass_delays[i]);
		if (d > REVERB_BUFFER_SIZE - 1) d = REVERB_BUFFER_SIZE - 1;
		int bufL = reverb_allpass_buffer_L[i][(reverb_pos - d) & REVERB_BUFFER_MASK];
		int bufR = reverb_allpass_buffer_R[i][(reverb_pos - d) & REVERB_BUFFER_MASK];

		reverb_allpass_buffer_L[i][reverb_pos & REVERB_BUFFER_MASK] = Clamp(ap_L + bufL / 2, BUF_LIMIT);
		reverb_allpass_buffer_R[i][reverb_pos & REVERB_BUFFER_MASK] = Clamp(ap_R + bufR / 2, BUF_LIMIT);

		ap_L = bufL - ap_L;
		ap_R = bufR - ap_R;
	}

	// Mix wet/dry based on reverb_mix (0-50%)
	int dry_level = 100 - g_Config.reverb_mix;
	int wet_level = g_Config.reverb_mix;

	L = (int)(((int64_t)L * dry_level + (int64_t)Clamp(ap_L, 65535) * wet_level) / 100);
	R = (int)(((int64_t)R * dry_level + (int64_t)Clamp(ap_R, 65535) * wet_level) / 100);

	reverb_pos++;
}

// ============================================================================
// Compressor
// ============================================================================
void AudioEffects::ProcessCompressor(int &L, int &R) {
	if (g_Config.compressor_level == 0) return;

	int level_L = FastAbs(L);
	int level_R = FastAbs(R);

	// Envelope follower (attack/release)
	if (level_L > comp_mid_env_L) {
		comp_mid_env_L += (level_L - comp_mid_env_L) >> 4;  // Fast attack
	} else {
		comp_mid_env_L -= (comp_mid_env_L - level_L) >> 8;  // Slow release
	}

	if (level_R > comp_mid_env_R) {
		comp_mid_env_R += (level_R - comp_mid_env_R) >> 4;
	} else {
		comp_mid_env_R -= (comp_mid_env_R - level_R) >> 8;
	}

	// Simple soft-knee compression (threshold around 50% of max)
	int threshold = 16384;  // ~50% of 16-bit range

	if (comp_mid_env_L > threshold) {
		int reduction = ((comp_mid_env_L - threshold) * g_Config.compressor_level) >> 4;
		if (reduction > 224) reduction = 224;  // Max 87.5% reduction to prevent signal inversion
		L = (L * (256 - reduction)) >> 8;
	}

	if (comp_mid_env_R > threshold) {
		int reduction = ((comp_mid_env_R - threshold) * g_Config.compressor_level) >> 4;
		if (reduction > 224) reduction = 224;  // Max 87.5% reduction to prevent signal inversion
		R = (R * (256 - reduction)) >> 8;
	}
}

// ============================================================================
// Main Processing Chain
// ============================================================================
void AudioEffects::ProcessEffects(int &out_L, int &out_R, int fm_L, int fm_R) {
	// Step 1: FM-specific effects, applied as a delta on the existing mix
	if (g_Config.fm_harmonic_level || g_Config.fm_warmth_level || g_Config.fm_chorus_depth) {
		int fL = fm_L, fR = fm_R;
		ProcessFMHarmonic(fL, fR);
		ProcessFMChorus(fL, fR);
		out_L += fL - fm_L;
		out_R += fR - fm_R;
	}

	// Step 2: Master effects chain
	ProcessHarmonicExciter(out_L, out_R);      // High frequency enhancement
	ProcessSubBass(out_L, out_R);              // Low frequency enhancement
	ProcessStereoImager(out_L, out_R);         // Stereo width
	ProcessReverb(out_L, out_R);               // Ambience/space
	ProcessCompressor(out_L, out_R);           // Dynamic range control
}

#endif // AUDIOEFFECTS_H

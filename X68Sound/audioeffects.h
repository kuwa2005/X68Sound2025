// ============================================================================
// Audio Effects Processor for X68Sound2025
// ============================================================================
// High-quality audio effects using integer arithmetic only
// All effects can be toggled via environment variables
// ============================================================================

#ifndef AUDIOEFFECTS_H
#define AUDIOEFFECTS_H

#include "global.h"

class AudioEffects {
private:
	// Reverb state variables
	int reverb_comb_buffer_L[REVERB_COMB_COUNT][REVERB_BUFFER_SIZE];
	int reverb_comb_buffer_R[REVERB_COMB_COUNT][REVERB_BUFFER_SIZE];
	int reverb_allpass_buffer_L[REVERB_ALLPASS_COUNT][REVERB_BUFFER_SIZE];
	int reverb_allpass_buffer_R[REVERB_ALLPASS_COUNT][REVERB_BUFFER_SIZE];
	int reverb_pos;

	// Reverb delay times (in samples, optimized for 48kHz)
	static const int comb_delays[REVERB_COMB_COUNT];
	static const int allpass_delays[REVERB_ALLPASS_COUNT];

	// Stereo processing state
	int stereo_delay_buffer_L[STEREO_DELAY_SIZE];
	int stereo_delay_buffer_R[STEREO_DELAY_SIZE];
	int stereo_delay_pos;

	// FM Chorus state
	int fm_chorus_buffer_L[CHORUS_BUFFER_SIZE];
	int fm_chorus_buffer_R[CHORUS_BUFFER_SIZE];
	int fm_chorus_pos;
	int fm_chorus_lfo_phase;

	// High-pass/Low-pass filter states for effects
	int exciter_highpass_L_prev;
	int exciter_highpass_R_prev;
	int exciter_highpass_L_out;
	int exciter_highpass_R_out;

	int bass_lowpass_L_prev;
	int bass_lowpass_R_prev;
	int bass_lowpass_L_out;
	int bass_lowpass_R_out;

	// Sub-harmonic generator state
	int subharm_phase_L;
	int subharm_phase_R;

	// Compressor state (envelope followers)
	int comp_low_env_L, comp_low_env_R;
	int comp_mid_env_L, comp_mid_env_R;
	int comp_high_env_L, comp_high_env_R;

	// Sample rate (for delay calculations)
	int sample_rate;

public:
	AudioEffects();
	void Init(int samprate);
	void Reset();

	// Main processing chain
	void ProcessEffects(int &out_L, int &out_R, int fm_L, int fm_R, int adpcm, int pcm8_L, int pcm8_R);

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
	sample_rate = samprate;
	Reset();
}

void AudioEffects::Reset() {
	// Clear reverb buffers
	for (int i = 0; i < REVERB_COMB_COUNT; i++) {
		for (int j = 0; j < REVERB_BUFFER_SIZE; j++) {
			reverb_comb_buffer_L[i][j] = 0;
			reverb_comb_buffer_R[i][j] = 0;
		}
	}
	for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
		for (int j = 0; j < REVERB_BUFFER_SIZE; j++) {
			reverb_allpass_buffer_L[i][j] = 0;
			reverb_allpass_buffer_R[i][j] = 0;
		}
	}
	reverb_pos = 0;

	// Clear stereo delay buffers
	for (int i = 0; i < STEREO_DELAY_SIZE; i++) {
		stereo_delay_buffer_L[i] = 0;
		stereo_delay_buffer_R[i] = 0;
	}
	stereo_delay_pos = 0;

	// Clear FM chorus buffers
	for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
		fm_chorus_buffer_L[i] = 0;
		fm_chorus_buffer_R[i] = 0;
	}
	fm_chorus_pos = 0;
	fm_chorus_lfo_phase = 0;

	// Clear filter states
	exciter_highpass_L_prev = exciter_highpass_R_prev = 0;
	exciter_highpass_L_out = exciter_highpass_R_out = 0;
	bass_lowpass_L_prev = bass_lowpass_R_prev = 0;
	bass_lowpass_L_out = bass_lowpass_R_out = 0;

	// Clear sub-harmonic state
	subharm_phase_L = subharm_phase_R = 0;

	// Clear compressor state
	comp_low_env_L = comp_low_env_R = 0;
	comp_mid_env_L = comp_mid_env_R = 0;
	comp_high_env_L = comp_high_env_R = 0;
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

	// Extract high frequency content with high-pass filter
	int high_L = ((L << 8) - (exciter_highpass_L_prev << 8) + 250 * exciter_highpass_L_out) >> 8;
	int high_R = ((R << 8) - (exciter_highpass_R_prev << 8) + 250 * exciter_highpass_R_out) >> 8;

	exciter_highpass_L_prev = L;
	exciter_highpass_R_prev = R;
	exciter_highpass_L_out = high_L;
	exciter_highpass_R_out = high_R;

	// Generate even harmonics (2nd and 3rd)
	int harmonic2_L = (high_L * FastAbs(high_L)) >> 16;
	int harmonic2_R = (high_R * FastAbs(high_R)) >> 16;

	int harmonic3_L = (harmonic2_L * high_L) >> 16;
	int harmonic3_R = (harmonic2_R * high_R) >> 16;

	// Mix harmonics based on intensity level (0-4)
	int intensity = g_Config.exciter_level << 4;  // 0, 16, 32, 48, 64
	L += ((harmonic2_L + (harmonic3_L >> 1)) * intensity) >> 8;
	R += ((harmonic2_R + (harmonic3_R >> 1)) * intensity) >> 8;
}

// ============================================================================
// Sub-Harmonic Bass - Low frequency enhancement
// ============================================================================
void AudioEffects::ProcessSubBass(int &L, int &R) {
	if (g_Config.sub_bass_level == 0) return;

	// Extract low frequency content with low-pass filter
	int low_L = (L + bass_lowpass_L_prev * 3) >> 2;
	int low_R = (R + bass_lowpass_R_prev * 3) >> 2;

	bass_lowpass_L_prev = low_L;
	bass_lowpass_R_prev = low_R;

	// Generate sub-harmonic (1 octave down) using phase accumulator
	subharm_phase_L = (subharm_phase_L + low_L) & 0xFFFF;
	subharm_phase_R = (subharm_phase_R + low_R) & 0xFFFF;

	int subharmonic_L = (subharm_phase_L > 32768) ? low_L : -low_L;
	int subharmonic_R = (subharm_phase_R > 32768) ? low_R : -low_R;

	// Mix sub-harmonic based on intensity level (0-4)
	int intensity = g_Config.sub_bass_level << 3;  // 0, 8, 16, 24, 32
	L += (subharmonic_L * intensity) >> 7;
	R += (subharmonic_R * intensity) >> 7;
}

// ============================================================================
// FM Harmonic Enhancer - Makes FM sound more analog
// ============================================================================
void AudioEffects::ProcessFMHarmonic(int &fm_L, int &fm_R) {
	if (g_Config.fm_harmonic_level == 0 && g_Config.fm_warmth_level == 0) return;

	// Generate even harmonics for warmth
	int harmonic2_L = (fm_L * fm_L) >> 16;
	int harmonic2_R = (fm_R * fm_R) >> 16;

	// Add 2nd harmonic (warmth)
	if (g_Config.fm_warmth_level > 0) {
		int warmth = g_Config.fm_warmth_level << 3;  // 0, 8, 16, 24, 32
		fm_L += (harmonic2_L * warmth) >> 8;
		fm_R += (harmonic2_R * warmth) >> 8;
	}

	// Add 3rd harmonic (richness)
	if (g_Config.fm_harmonic_level > 0) {
		int harmonic3_L = (fm_L * harmonic2_L) >> 16;
		int harmonic3_R = (fm_R * harmonic2_R) >> 16;

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

	// Store input in delay buffer
	fm_chorus_buffer_L[fm_chorus_pos & CHORUS_BUFFER_MASK] = fm_L;
	fm_chorus_buffer_R[fm_chorus_pos & CHORUS_BUFFER_MASK] = fm_R;

	// LFO for chorus modulation (use existing sine table from OPM)
	fm_chorus_lfo_phase += 2;  // Slow LFO ~1Hz @ 48kHz
	int lfo = SINTBL[(fm_chorus_lfo_phase >> 2) & (SIZESINTBL-1)];

	// Delay time based on chorus depth (1=Subtle, 2=Rich, 3=Wide)
	int base_delay = 50 + (g_Config.fm_chorus_depth * 30);  // 80, 110, 140 samples
	int modulation = (lfo * g_Config.fm_chorus_depth) >> 10;  // ±20, ±40, ±60 samples
	int delay_time = base_delay + modulation;

	// Read from delayed position
	int delayed_L = fm_chorus_buffer_L[(fm_chorus_pos - delay_time) & CHORUS_BUFFER_MASK];
	int delayed_R = fm_chorus_buffer_R[(fm_chorus_pos - delay_time) & CHORUS_BUFFER_MASK];

	// Mix dry and wet (50/50 for chorus effect)
	fm_L = (fm_L + delayed_L) >> 1;
	fm_R = (fm_R + delayed_R) >> 1;

	fm_chorus_pos++;
}

// ============================================================================
// Stereo Imager - 3D surround effect with Haas and crossfeed
// ============================================================================
void AudioEffects::ProcessStereoImager(int &L, int &R) {
	// Store current samples
	stereo_delay_buffer_L[stereo_delay_pos & STEREO_DELAY_MASK] = L;
	stereo_delay_buffer_R[stereo_delay_pos & STEREO_DELAY_MASK] = R;

	// Mid/Side processing for width control
	if (g_Config.stereo_width != 100) {
		int mid = (L + R) >> 1;
		int side = (L - R) >> 1;

		// Adjust width (100=normal, 150=wide, 200=ultra wide)
		side = (side * g_Config.stereo_width) >> 7;  // Divide by 128 for percentage

		L = mid + side;
		R = mid - side;
	}

	// Haas effect for spatial enhancement (3-4ms delay)
	int haas_delay_samples = 3 * sample_rate / 1000;  // 3ms
	int delayed_L = stereo_delay_buffer_L[(stereo_delay_pos - haas_delay_samples) & STEREO_DELAY_MASK];
	int delayed_R = stereo_delay_buffer_R[(stereo_delay_pos - haas_delay_samples) & STEREO_DELAY_MASK];

	// Crossfeed for headphone listening (reduces center image fatigue)
	if (g_Config.crossfeed_level > 0) {
		int crossfeed_amount = g_Config.crossfeed_level;  // 0-100
		L -= (delayed_R * crossfeed_amount) >> 9;
		R -= (delayed_L * crossfeed_amount) >> 9;
	}

	stereo_delay_pos++;
}

// ============================================================================
// Ambience Reverb - Freeverb algorithm implementation
// ============================================================================
void AudioEffects::ProcessReverb(int &L, int &R) {
	if (g_Config.reverb_type == 0) return;

	// Comb filters (parallel)
	int comb_out_L = 0, comb_out_R = 0;
	int decay = g_Config.reverb_decay;  // 10-95

	for (int i = 0; i < REVERB_COMB_COUNT; i++) {
		// Scale delay time based on reverb type (1=Small, 2=Medium, 3=Large, 4=Hall)
		int delay = (comb_delays[i] * g_Config.reverb_type) >> 1;

		int comb_delayed_L = reverb_comb_buffer_L[i][(reverb_pos - delay) & REVERB_BUFFER_MASK];
		int comb_delayed_R = reverb_comb_buffer_R[i][(reverb_pos - delay) & REVERB_BUFFER_MASK];

		// Feedback with decay
		int feedback_L = (comb_delayed_L * decay) >> 7;  // Divide by 128
		int feedback_R = (comb_delayed_R * decay) >> 7;

		reverb_comb_buffer_L[i][reverb_pos & REVERB_BUFFER_MASK] = L + feedback_L;
		reverb_comb_buffer_R[i][reverb_pos & REVERB_BUFFER_MASK] = R + feedback_R;

		comb_out_L += comb_delayed_L;
		comb_out_R += comb_delayed_R;
	}

	// Average comb outputs
	comb_out_L >>= 3;  // Divide by 8
	comb_out_R >>= 3;

	// All-pass filters (series) for diffusion
	int allpass_out_L = comb_out_L;
	int allpass_out_R = comb_out_R;

	for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
		int delayed_L = reverb_allpass_buffer_L[i][(reverb_pos - allpass_delays[i]) & REVERB_BUFFER_MASK];
		int delayed_R = reverb_allpass_buffer_R[i][(reverb_pos - allpass_delays[i]) & REVERB_BUFFER_MASK];

		int temp_L = delayed_L - (allpass_out_L >> 1);
		int temp_R = delayed_R - (allpass_out_R >> 1);

		reverb_allpass_buffer_L[i][reverb_pos & REVERB_BUFFER_MASK] = allpass_out_L;
		reverb_allpass_buffer_R[i][reverb_pos & REVERB_BUFFER_MASK] = allpass_out_R;

		allpass_out_L = delayed_L + (temp_L >> 1);
		allpass_out_R = delayed_R + (temp_R >> 1);
	}

	// Mix wet/dry based on reverb_mix (0-50%)
	int dry_level = 100 - g_Config.reverb_mix;
	int wet_level = g_Config.reverb_mix;

	L = ((L * dry_level) + (allpass_out_L * wet_level)) / 100;
	R = ((R * dry_level) + (allpass_out_R * wet_level)) / 100;

	reverb_pos++;
}

// ============================================================================
// Multiband Compressor - Professional mastering quality
// ============================================================================
void AudioEffects::ProcessCompressor(int &L, int &R) {
	if (g_Config.compressor_level == 0) return;

	// Simple envelope follower for now (placeholder for full implementation)
	// This is a simplified version - full multiband would split frequencies

	int level_L = FastAbs(L);
	int level_R = FastAbs(R);

	// Envelope follower (attack/release)
	int attack = 1 << (g_Config.compressor_level + 3);
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
		L = (L * (256 - reduction)) >> 8;
	}

	if (comp_mid_env_R > threshold) {
		int reduction = ((comp_mid_env_R - threshold) * g_Config.compressor_level) >> 4;
		R = (R * (256 - reduction)) >> 8;
	}
}

// ============================================================================
// Main Processing Chain
// ============================================================================
void AudioEffects::ProcessEffects(int &out_L, int &out_R, int fm_L, int fm_R, int adpcm, int pcm8_L, int pcm8_R) {
	// Step 1: FM-specific effects (before mixing)
	ProcessFMHarmonic(fm_L, fm_R);
	ProcessFMChorus(fm_L, fm_R);

	// Step 2: Mix all sources
	out_L = fm_L + pcm8_L + adpcm;
	out_R = fm_R + pcm8_R + adpcm;

	// Step 3: Master effects chain
	ProcessHarmonicExciter(out_L, out_R);      // High frequency enhancement
	ProcessSubBass(out_L, out_R);              // Low frequency enhancement
	ProcessStereoImager(out_L, out_R);         // 3D spatial processing
	ProcessReverb(out_L, out_R);               // Ambience/space
	ProcessCompressor(out_L, out_R);           // Dynamic range control
}

#endif // AUDIOEFFECTS_H

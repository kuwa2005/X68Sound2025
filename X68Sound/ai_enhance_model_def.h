// =============================================================================
// Neural ADPCM enhancer - model definition shared by the trainer and runtime
// (tools/ai/train_adpcm_enhancer.cpp, X68Sound/ai_enhance.h)
// =============================================================================
#ifndef X68_AI_ENHANCE_MODEL_DEF_H
#define X68_AI_ENHANCE_MODEL_DEF_H

#include <math.h>

#define AI_PAST    16                         // past samples in the window
#define AI_FUTURE  8                          // future samples (output latency)
#define AI_WIN     (AI_PAST + 1 + AI_FUTURE)  // window length
#define AI_NIN     (2 * AI_WIN + 2)           // input features
#define AI_H1      40
#define AI_H2      40

// Build the input feature vector for the window centered at index c.
//   dec[]   : decoded ADPCM values (12-bit scale)
//   lstep[] : log2 of the ADPCM step sizes
// Features are normalized by the center step size, so the model is amplitude
// independent (ADPCM quantization error scales with the step size).
static inline void AiBuildInput(const float *dec, const float *lstep, int c, float *in) {
	float sc = exp2f(lstep[c]);
	float inv = 1.0f / (4.0f * sc);
	int k = 0;
	for (int j = -AI_PAST; j <= AI_FUTURE; ++j) {
		float v = (dec[c + j] - dec[c]) * inv;
		if (v > 4) v = 4; if (v < -4) v = -4;
		in[k++] = v;
	}
	for (int j = -AI_PAST; j <= AI_FUTURE; ++j) {
		in[k++] = (lstep[c + j] - lstep[c]) * (1.0f / 3.0f);
	}
	in[k++] = dec[c] * (1.0f / 2048.0f);
	in[k++] = sc * (1.0f / 1552.0f);
}

#endif

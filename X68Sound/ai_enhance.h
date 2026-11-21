// =============================================================================
// Neural ADPCM enhancer (runtime) - X68SOUND_AI_ENHANCE=1
// =============================================================================
// A small MLP (see ai_enhance_model_def.h) trained on synthetic material
// (tools/ai/train_adpcm_enhancer.cpp) predicts the ADPCM quantization error of
// the decoded sample and subtracts it.
//  - Applied per ADPCM channel (MSM6258 and PCM8 ADPCM), legacy decoder only
//  - Output latency: AI_FUTURE input samples
//  - Enabled only for offline rendering (X68Sound_StartPcm, e.g. mxwav):
//    it is too heavy to guarantee real-time playback on every machine.
// =============================================================================
#ifndef X68_AI_ENHANCE_H
#define X68_AI_ENHANCE_H

#include <string.h>
#include <math.h>
#include "ai_enhance_model_def.h"
#include "ai_model.h"

class NeuralAdpcmEnhancer {
public:
	NeuralAdpcmEnhancer() { Reset(); }

	void Reset() {
		for (int i = 0; i < AI_WIN; ++i) { dec[i] = 0; lstep[i] = 4.0f; }   // log2(16)
	}

	// Push one decoded sample (12-bit scale) and the ADPCM step used to decode
	// it; returns the enhanced sample AI_FUTURE samples back in time.
	inline float Process(float decoded, float step, float strength) {
		memmove(dec, dec + 1, sizeof(float) * (AI_WIN - 1));
		memmove(lstep, lstep + 1, sizeof(float) * (AI_WIN - 1));
		dec[AI_WIN - 1] = decoded;
		lstep[AI_WIN - 1] = log2f(step > 1 ? step : 1);

		float in[AI_NIN], h1[AI_H1], h2[AI_H2];
		AiBuildInput(dec, lstep, AI_PAST, in);
		for (int j = 0; j < AI_H1; ++j) {
			float s = AI_B1[j]; const float *w = &AI_W1[j * AI_NIN];
			for (int i = 0; i < AI_NIN; ++i) s += w[i] * in[i];
			h1[j] = tanhf(s);
		}
		for (int j = 0; j < AI_H2; ++j) {
			float s = AI_B2[j]; const float *w = &AI_W2[j * AI_H1];
			for (int i = 0; i < AI_H1; ++i) s += w[i] * h1[i];
			h2[j] = tanhf(s);
		}
		float r = AI_B3[0];
		for (int i = 0; i < AI_H2; ++i) r += AI_W3[i] * h2[i];
		// the correction can never exceed one quantization step
		if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;
		float y = dec[AI_PAST] + strength * r * exp2f(lstep[AI_PAST]);
		if (y > 2047) y = 2047; else if (y < -2047) y = -2047;
		return y;
	}

private:
	float dec[AI_WIN];
	float lstep[AI_WIN];
};

#endif

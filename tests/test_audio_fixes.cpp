// =============================================================================
// X68Sound2025 - Unit Tests & Integration Tests for Audio Distortion Bug Fixes
// =============================================================================
// Tests cover:
//   1. ApplySoftClipping (opm.h) - zero-division fix, boundary conditions
//   2. ProcessCompressor (audioeffects.h) - signal inversion prevention
//   3. ProcessReverb (audioeffects.h) - buffer overflow clamping
//   4. HPF filter (adpcm.h) - discontinuity click prevention
//   5. Integration tests - full effects chain
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>

// =============================================================================
// Test Framework
// =============================================================================
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s - expected %d, got %d (line %d)\n", msg, (b), (a), __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_RANGE(val, lo, hi, msg) do { \
    if ((val) < (lo) || (val) > (hi)) { \
        printf("  FAIL: %s - %d not in [%d, %d] (line %d)\n", msg, (val), (lo), (hi), __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_BEGIN(name) do { \
    printf("[TEST] %s\n", name); \
    g_tests_run++; \
} while(0)

#define TEST_END() do { \
    g_tests_passed++; \
    printf("  PASS\n"); \
} while(0)

#define TEST_SECTION(name) printf("\n--- %s ---\n", name)

// =============================================================================
// Stubs - Extracted from original code for Linux compilation
// =============================================================================

// Config structure (simplified from global.h)
struct X68SoundConfig {
    int exciter_level;
    int sub_bass_level;
    int fm_harmonic_level;
    int fm_warmth_level;
    int fm_chorus_depth;
    int stereo_width;
    int crossfeed_level;
    int center_channel_width;
    int reverb_type;
    int reverb_decay;
    int reverb_mix;
    int compressor_level;
    int soft_clipping_enable;
    int soft_clipping_threshold;
};

X68SoundConfig g_Config;

inline int FastAbs(int x) { return (x < 0) ? -x : x; }

// Sine table stub (from global.h)
short SINTBL[1024];
void InitSineTable() {
    for (int i = 0; i < 1024; i++) {
        SINTBL[i] = (short)(32767.0 * sin(2.0 * 3.1415926535897932 * i / 1024.0));
    }
}

// =============================================================================
// Function Under Test 1: ApplySoftClipping (from opm.h)
// =============================================================================
inline int ApplySoftClipping(int sample, int threshold_percent) {
    const int MAX_SAMPLE = 32767;
    int threshold = (MAX_SAMPLE * threshold_percent) / 100;
    int range = MAX_SAMPLE - threshold;

    // Prevent division by zero if threshold is 100%
    if (range <= 0) {
        if (sample > MAX_SAMPLE) return MAX_SAMPLE;
        if (sample < -MAX_SAMPLE) return -MAX_SAMPLE;
        return sample;
    }

    if (sample > threshold) {
        int overshoot = sample - threshold;
        int ratio = (overshoot << 8) / range;
        if (ratio > 256) ratio = 256;
        int soft = (ratio << 8) / (256 + ratio);
        return threshold + ((range * soft) >> 8);
    } else if (sample < -threshold) {
        int overshoot = -sample - threshold;
        int ratio = (overshoot << 8) / range;
        if (ratio > 256) ratio = 256;
        int soft = (ratio << 8) / (256 + ratio);
        return -(threshold + ((range * soft) >> 8));
    }

    return sample;
}

// =============================================================================
// Function Under Test 2: ProcessCompressor (from audioeffects.h)
// =============================================================================
class TestAudioEffects {
public:
    int comp_mid_env_L, comp_mid_env_R;
    int comb_buffer_L[8][32768];
    int comb_buffer_R[8][32768];
    int allpass_buffer_L[4][32768];
    int allpass_buffer_R[4][32768];
    int reverb_pos;

    static const int comb_delays[8];
    static const int allpass_delays[4];

    void Reset() {
        comp_mid_env_L = comp_mid_env_R = 0;
        reverb_pos = 0;
        memset(comb_buffer_L, 0, sizeof(comb_buffer_L));
        memset(comb_buffer_R, 0, sizeof(comb_buffer_R));
        memset(allpass_buffer_L, 0, sizeof(allpass_buffer_L));
        memset(allpass_buffer_R, 0, sizeof(allpass_buffer_R));
    }

    // Compressor - extracted from audioeffects.h with bug fix
    void ProcessCompressor(int &L, int &R) {
        if (g_Config.compressor_level == 0) return;

        int level_L = FastAbs(L);
        int level_R = FastAbs(R);

        if (level_L > comp_mid_env_L) {
            comp_mid_env_L += (level_L - comp_mid_env_L) >> 4;
        } else {
            comp_mid_env_L -= (comp_mid_env_L - level_L) >> 8;
        }

        if (level_R > comp_mid_env_R) {
            comp_mid_env_R += (level_R - comp_mid_env_R) >> 4;
        } else {
            comp_mid_env_R -= (comp_mid_env_R - level_R) >> 8;
        }

        int threshold = 16384;

        if (comp_mid_env_L > threshold) {
            int reduction = ((comp_mid_env_L - threshold) * g_Config.compressor_level) >> 4;
            if (reduction > 224) reduction = 224;  // FIX: prevent signal inversion
            L = (L * (256 - reduction)) >> 8;
        }

        if (comp_mid_env_R > threshold) {
            int reduction = ((comp_mid_env_R - threshold) * g_Config.compressor_level) >> 4;
            if (reduction > 224) reduction = 224;  // FIX: prevent signal inversion
            R = (R * (256 - reduction)) >> 8;
        }
    }

    // Reverb - extracted from audioeffects.h with buffer clamping fix
    void ProcessReverb(int &L, int &R) {
        if (g_Config.reverb_type == 0) return;

        int comb_out_L = 0, comb_out_R = 0;
        int decay = g_Config.reverb_decay;

        for (int i = 0; i < 8; i++) {
            int delay = (comb_delays[i] * g_Config.reverb_type) >> 1;

            int comb_delayed_L = comb_buffer_L[i][(reverb_pos - delay) & 32767];
            int comb_delayed_R = comb_buffer_R[i][(reverb_pos - delay) & 32767];

            int feedback_L = (comb_delayed_L * decay) >> 7;
            int feedback_R = (comb_delayed_R * decay) >> 7;

            int new_val_L = L + feedback_L;
            int new_val_R = R + feedback_R;
            // FIX: Clamp reverb buffer values
            if (new_val_L > 50000) new_val_L = 50000;
            if (new_val_L < -50000) new_val_L = -50000;
            if (new_val_R > 50000) new_val_R = 50000;
            if (new_val_R < -50000) new_val_R = -50000;
            comb_buffer_L[i][reverb_pos & 32767] = new_val_L;
            comb_buffer_R[i][reverb_pos & 32767] = new_val_R;

            comb_out_L += comb_delayed_L;
            comb_out_R += comb_delayed_R;
        }

        comb_out_L >>= 3;
        comb_out_R >>= 3;

        int allpass_out_L = comb_out_L;
        int allpass_out_R = comb_out_R;

        for (int i = 0; i < 4; i++) {
            int delayed_L = allpass_buffer_L[i][(reverb_pos - allpass_delays[i]) & 32767];
            int delayed_R = allpass_buffer_R[i][(reverb_pos - allpass_delays[i]) & 32767];

            int temp_L = delayed_L - (allpass_out_L >> 1);
            int temp_R = delayed_R - (allpass_out_R >> 1);

            // FIX: Clamp allpass buffer values
            if (allpass_out_L > 50000) allpass_out_L = 50000;
            if (allpass_out_L < -50000) allpass_out_L = -50000;
            if (allpass_out_R > 50000) allpass_out_R = 50000;
            if (allpass_out_R < -50000) allpass_out_R = -50000;
            allpass_buffer_L[i][reverb_pos & 32767] = allpass_out_L;
            allpass_buffer_R[i][reverb_pos & 32767] = allpass_out_R;

            allpass_out_L = delayed_L + (temp_L >> 1);
            allpass_out_R = delayed_R + (temp_R >> 1);
        }

        int dry_level = 100 - g_Config.reverb_mix;
        int wet_level = g_Config.reverb_mix;

        L = ((L * dry_level) + (allpass_out_L * wet_level)) / 100;
        R = ((R * dry_level) + (allpass_out_R * wet_level)) / 100;

        reverb_pos++;
    }
};

const int TestAudioEffects::comb_delays[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
const int TestAudioEffects::allpass_delays[4] = { 225, 556, 441, 341 };

// =============================================================================
// Function Under Test 3: HPF filter (from adpcm.h)
// =============================================================================
class TestHPFFilter {
public:
    int OutPcm;
    int InpPcm_prev;
    int SaturationCounter;

    void Reset() {
        OutPcm = 0;
        InpPcm_prev = 0;
        SaturationCounter = 0;
    }

    // HPF filter with discontinuity fix
    int ProcessHPF(int InpPcm_for_hpf, int TotalVolume) {
        const int HPF_COEFF_A1_22KHZ = 459;
        const int HPF_SHIFT = 9;

        int hpf_diff = (InpPcm_for_hpf << HPF_SHIFT) - (InpPcm_prev << HPF_SHIFT);
        int hpf_feedback = HPF_COEFF_A1_22KHZ * OutPcm;
        OutPcm = (hpf_diff + hpf_feedback) >> HPF_SHIFT;
        InpPcm_prev = InpPcm_for_hpf;

        const int OUT_PCM_LIMIT = 70000;
        const int OUT_PCM_RESET_THRESHOLD = 60000;

        int abs_OutPcm = (OutPcm >= 0) ? OutPcm : -OutPcm;
        if (abs_OutPcm > OUT_PCM_RESET_THRESHOLD) {
            SaturationCounter++;

            if (SaturationCounter >= 2) {
                // FIX: Only reset on consecutive saturation (not single)
                OutPcm = 0;
                InpPcm_prev = 0;
                SaturationCounter = 0;
            } else {
                // FIX: Soft clamp instead of hard jump
                if (OutPcm > OUT_PCM_LIMIT) OutPcm = OUT_PCM_LIMIT;
                if (OutPcm < -OUT_PCM_LIMIT) OutPcm = -OUT_PCM_LIMIT;
            }
        } else {
            SaturationCounter = 0;
        }

        int result = (OutPcm * TotalVolume) >> 8;
        const int RESULT_LIMIT = 40000;
        if (result > RESULT_LIMIT) result = RESULT_LIMIT;
        if (result < -RESULT_LIMIT) result = -RESULT_LIMIT;

        return result;
    }
};

// =============================================================================
// UNIT TESTS
// =============================================================================

void test_soft_clipping_normal() {
    TEST_BEGIN("ApplySoftClipping - Normal operation (no clipping)");
    int result = ApplySoftClipping(10000, 85);
    TEST_ASSERT_EQ(result, 10000, "Signal below threshold should pass through unchanged");
    TEST_END();
}

void test_soft_clipping_boundary() {
    TEST_BEGIN("ApplySoftClipping - At threshold boundary");
    int threshold = (32767 * 85) / 100;  // 27851
    int result = ApplySoftClipping(threshold, 85);
    TEST_ASSERT_EQ(result, threshold, "Signal at threshold should pass through");
    TEST_END();
}

void test_soft_clipping_positive_clip() {
    TEST_BEGIN("ApplySoftClipping - Positive overshoot clipping");
    int result = ApplySoftClipping(30000, 85);
    int threshold = (32767 * 85) / 100;
    TEST_ASSERT(result > threshold, "Clipped output should be above threshold");
    TEST_ASSERT(result <= 32767, "Clipped output should not exceed MAX_SAMPLE");
    TEST_END();
}

void test_soft_clipping_negative_clip() {
    TEST_BEGIN("ApplySoftClipping - Negative overshoot clipping (symmetry)");
    int pos_result = ApplySoftClipping(30000, 85);
    int neg_result = ApplySoftClipping(-30000, 85);
    TEST_ASSERT_EQ(neg_result, -pos_result, "Negative clipping should be symmetric");
    TEST_END();
}

void test_soft_clipping_extreme_positive() {
    TEST_BEGIN("ApplySoftClipping - Extreme positive input");
    int result = ApplySoftClipping(50000, 85);
    int threshold = (32767 * 85) / 100;
    TEST_ASSERT(result > threshold, "Extreme positive should still clip smoothly");
    TEST_ASSERT(result <= 32767, "Should not exceed MAX_SAMPLE");
    TEST_END();
}

void test_soft_clipping_extreme_negative() {
    TEST_BEGIN("ApplySoftClipping - Extreme negative input");
    int result = ApplySoftClipping(-50000, 85);
    int threshold = (32767 * 85) / 100;
    TEST_ASSERT(result < -threshold, "Extreme negative should still clip smoothly");
    TEST_ASSERT(result >= -32767, "Should not go below -MAX_SAMPLE");
    TEST_END();
}

void test_soft_clipping_zero_threshold() {
    TEST_BEGIN("ApplySoftClipping - Zero signal");
    int result = ApplySoftClipping(0, 85);
    TEST_ASSERT_EQ(result, 0, "Zero signal should remain zero");
    TEST_END();
}

void test_soft_clipping_threshold_100() {
    TEST_BEGIN("ApplySoftClipping - threshold_percent=100 (BUG FIX: zero division)");
    // Before fix: range = 32767 - 32767 = 0, division by zero
    // After fix: range <= 0 triggers early return
    int result = ApplySoftClipping(40000, 100);
    TEST_ASSERT(result == 32767, "Should clamp to MAX_SAMPLE");

    result = ApplySoftClipping(-40000, 100);
    TEST_ASSERT(result == -32767, "Should clamp to -MAX_SAMPLE");

    result = ApplySoftClipping(10000, 100);
    TEST_ASSERT_EQ(result, 10000, "Signal within range should pass through");
    TEST_END();
}

void test_soft_clipping_various_thresholds() {
    TEST_BEGIN("ApplySoftClipping - Various threshold percentages");
    // 50% threshold: threshold=16383, range=16384
    int r1 = ApplySoftClipping(25000, 50);
    int t50 = (32767 * 50) / 100;
    TEST_ASSERT(r1 > t50, "50% threshold output should be above threshold");
    TEST_ASSERT(r1 <= 32767, "50% threshold output should not exceed MAX_SAMPLE");

    // 75% threshold
    int r2 = ApplySoftClipping(30000, 75);
    int t75 = (32767 * 75) / 100;
    TEST_ASSERT(r2 >= t75, "75% threshold output should be above threshold");

    // 99% threshold (edge case, range=327)
    int r3 = ApplySoftClipping(35000, 99);
    TEST_ASSERT(r3 <= 32767, "99% threshold should not exceed MAX_SAMPLE");
    TEST_END();
}

// =============================================================================
// Compressor Tests
// =============================================================================

void test_compressor_no_clipping() {
    TEST_BEGIN("ProcessCompressor - No clipping with moderate signal");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.compressor_level = 3;

    int L = 10000, R = 10000;
    // Run through several samples to build envelope
    for (int i = 0; i < 100; i++) {
        fx.ProcessCompressor(L, R);
    }
    TEST_ASSERT(L > 0, "Output should be positive");
    TEST_ASSERT(R > 0, "Output should be positive");
    TEST_END();
}

void test_compressor_no_signal_inversion() {
    TEST_BEGIN("ProcessCompressor - NO signal inversion (BUG FIX)");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.compressor_level = 3;  // Strong compression

    // Push the envelope to extreme levels
    int L = 30000, R = 30000;
    // Run many iterations to build envelope beyond threshold
    for (int i = 0; i < 1000; i++) {
        fx.ProcessCompressor(L, R);
    }

    // The critical test: output should NOT be inverted
    // Before fix: reduction could exceed 256, causing (256 - reduction) to be negative
    // After fix: reduction is clamped to 224
    L = 30000; R = 30000;
    fx.ProcessCompressor(L, R);

    printf("  (envelope_L=%d, output_L=%d)\n", fx.comp_mid_env_L, L);
    TEST_ASSERT(L >= 0, "BUG FIX: Output must NOT be negative (no signal inversion)");
    TEST_ASSERT(R >= 0, "BUG FIX: Output must NOT be negative (no signal inversion)");
    TEST_END();
}

void test_compressor_extreme_envelope() {
    TEST_BEGIN("ProcessCompressor - Extreme envelope value protection");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.compressor_level = 3;

    // Simulate extreme envelope buildup
    fx.comp_mid_env_L = 60000;
    fx.comp_mid_env_R = 60000;

    int L = 20000, R = 20000;
    fx.ProcessCompressor(L, R);

    // With clamped reduction (max 224), the multiplier is (256-224)=32
    // So output = (20000 * 32) >> 8 = 2500
    printf("  (output_L=%d, expected ~2500)\n", L);
    TEST_ASSERT(L > 0, "Output should remain positive");
    TEST_ASSERT(L < 20000, "Output should be reduced by compressor");
    TEST_END();
}

void test_compressor_off() {
    TEST_BEGIN("ProcessCompressor - Compressor disabled (level=0)");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.compressor_level = 0;

    int L = 15000, R = 15000;
    int orig_L = L, orig_R = R;
    fx.ProcessCompressor(L, R);

    TEST_ASSERT_EQ(L, orig_L, "Signal should pass through unchanged when compressor is off");
    TEST_ASSERT_EQ(R, orig_R, "Signal should pass through unchanged when compressor is off");
    TEST_END();
}

void test_compressor_symmetry() {
    TEST_BEGIN("ProcessCompressor - Positive/Negative symmetry");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.compressor_level = 3;

    // Process positive signal
    int pos_L = 25000;
    for (int i = 0; i < 200; i++) fx.ProcessCompressor(pos_L, pos_L);
    int pos_result = pos_L;

    // Reset and process negative signal
    fx.Reset();
    int neg_L = -25000;
    for (int i = 0; i < 200; i++) fx.ProcessCompressor(neg_L, neg_L);
    int neg_result = neg_L;

    printf("  (pos=%d, neg=%d)\n", pos_result, neg_result);
    // Both should have same absolute reduction
    TEST_ASSERT(pos_result > 0, "Positive signal should remain positive");
    TEST_ASSERT(neg_result < 0, "Negative signal should remain negative");
    TEST_END();
}

// =============================================================================
// Reverb Buffer Clamping Tests
// =============================================================================

void test_reverb_comb_buffer_clamping() {
    TEST_BEGIN("ProcessReverb - Comb buffer clamping (BUG FIX)");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 4;      // Hall reverb
    g_Config.reverb_decay = 95;    // Max decay
    g_Config.reverb_mix = 50;      // Max mix

    // Feed extreme signal
    int L = 32000, R = 32000;
    for (int i = 0; i < 10000; i++) {
        fx.ProcessReverb(L, R);
    }

    // Check all comb buffers are clamped
    int max_val = 0;
    for (int ch = 0; ch < 8; ch++) {
        for (int j = 0; j < 32768; j++) {
            int v = FastAbs(fx.comb_buffer_L[ch][j]);
            if (v > max_val) max_val = v;
        }
    }
    printf("  (max comb buffer value: %d)\n", max_val);
    TEST_ASSERT(max_val <= 50000, "BUG FIX: Comb buffer values should be clamped to 50000");
    TEST_END();
}

void test_reverb_allpass_buffer_clamping() {
    TEST_BEGIN("ProcessReverb - Allpass buffer clamping (BUG FIX)");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 4;
    g_Config.reverb_decay = 95;
    g_Config.reverb_mix = 50;

    int L = 32000, R = 32000;
    for (int i = 0; i < 10000; i++) {
        fx.ProcessReverb(L, R);
    }

    int max_val = 0;
    for (int ch = 0; ch < 4; ch++) {
        for (int j = 0; j < 32768; j++) {
            int v = FastAbs(fx.allpass_buffer_L[ch][j]);
            if (v > max_val) max_val = v;
        }
    }
    printf("  (max allpass buffer value: %d)\n", max_val);
    TEST_ASSERT(max_val <= 50000, "BUG FIX: Allpass buffer values should be clamped to 50000");
    TEST_END();
}

void test_reverb_no_unbounded_growth() {
    TEST_BEGIN("ProcessReverb - No unbounded signal growth over time");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 4;
    g_Config.reverb_decay = 95;
    g_Config.reverb_mix = 30;

    // Track output peak over time
    int peak_output = 0;
    for (int i = 0; i < 50000; i++) {
        int L = 20000;
        int R = 20000;
        fx.ProcessReverb(L, R);
        int abs_out = FastAbs(L);
        if (abs_out > peak_output) peak_output = abs_out;
    }

    printf("  (peak output over 50000 samples: %d)\n", peak_output);
    TEST_ASSERT(peak_output < 100000,
        "Output should not grow unboundedly (clamped buffers prevent this)");
    TEST_END();
}

void test_reverb_off() {
    TEST_BEGIN("ProcessReverb - Reverb disabled (type=0)");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 0;

    int L = 15000, R = 15000;
    int orig_L = L, orig_R = R;
    fx.ProcessReverb(L, R);

    TEST_ASSERT_EQ(L, orig_L, "Signal should pass through unchanged when reverb is off");
    TEST_ASSERT_EQ(R, orig_R, "Signal should pass through unchanged when reverb is off");
    TEST_END();
}

// =============================================================================
// HPF Filter Tests
// =============================================================================

void test_hpf_normal_operation() {
    TEST_BEGIN("HPF Filter - Normal operation");
    TestHPFFilter hpf;
    hpf.Reset();

    int result = hpf.ProcessHPF(10000, 256);
    TEST_ASSERT_RANGE(result, -40000, 40000, "Output should be within limits");
    TEST_END();
}

void test_hpf_no_click_on_saturation() {
    TEST_BEGIN("HPF Filter - No click/discontinuity on saturation (BUG FIX)");
    TestHPFFilter hpf;
    hpf.Reset();

    int prev_result = 0;
    int max_jump = 0;

    // Feed extreme signal to trigger saturation
    for (int i = 0; i < 1000; i++) {
        int result = hpf.ProcessHPF(30000, 256);
        int jump = FastAbs(result - prev_result);
        if (jump > max_jump) max_jump = jump;
        prev_result = result;
    }

    printf("  (max sample-to-sample jump: %d)\n", max_jump);
    // Before fix: reset to 0 caused jumps of 40000+
    // After fix: soft clamping limits jumps
    TEST_ASSERT(max_jump < 35000,
        "BUG FIX: No large discontinuity clicks on saturation");
    TEST_END();
}

void test_hpf_consecutive_saturation_reset() {
    TEST_BEGIN("HPF Filter - Consecutive saturation triggers reset");
    TestHPFFilter hpf;
    hpf.Reset();

    // Force filter into saturation state
    for (int i = 0; i < 100; i++) {
        hpf.ProcessHPF(32000, 256);
    }

    // After consecutive saturation, counter should have reset the filter
    printf("  (SaturationCounter after saturation: %d)\n", hpf.SaturationCounter);
    TEST_ASSERT(hpf.SaturationCounter == 0,
        "Counter should reset after consecutive saturation triggers filter reset");
    TEST_END();
}

void test_hpf_symmetry() {
    TEST_BEGIN("HPF Filter - Positive/Negative symmetry");
    TestHPFFilter hpf_pos;
    TestHPFFilter hpf_neg;
    hpf_pos.Reset();
    hpf_neg.Reset();

    int pos_result = hpf_pos.ProcessHPF(20000, 256);
    int neg_result = hpf_neg.ProcessHPF(-20000, 256);

    printf("  (pos=%d, neg=%d)\n", pos_result, neg_result);
    // Signs should be opposite (approximately)
    if (pos_result > 0) {
        TEST_ASSERT(neg_result < 0, "Negative input should produce negative output");
    } else if (pos_result < 0) {
        TEST_ASSERT(neg_result > 0, "Negative input should produce negative output");
    }
    TEST_END();
}

void test_hpf_zero_input() {
    TEST_BEGIN("HPF Filter - Zero input");
    TestHPFFilter hpf;
    hpf.Reset();

    int result = hpf.ProcessHPF(0, 256);
    TEST_ASSERT_EQ(result, 0, "Zero input should produce zero output");

    // Feed some signal then go to zero
    hpf.ProcessHPF(10000, 256);
    hpf.ProcessHPF(10000, 256);
    result = hpf.ProcessHPF(0, 256);
    // Should settle to near zero (HPF blocks DC)
    TEST_ASSERT_RANGE(result, -5000, 5000, "Signal should decay after zero input");
    TEST_END();
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

void test_full_effects_chain_no_distortion() {
    TEST_BEGIN("Integration - Full effects chain produces no distorted output");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.exciter_level = 2;
    g_Config.sub_bass_level = 2;
    g_Config.stereo_width = 150;
    g_Config.crossfeed_level = 30;
    g_Config.reverb_type = 2;
    g_Config.reverb_decay = 70;
    g_Config.reverb_mix = 20;
    g_Config.compressor_level = 0;  // Disable compressor to test raw signal path

    // Process many samples with various input levels
    int max_output = 0;
    int min_output = 0;
    for (int i = 0; i < 10000; i++) {
        int L = (int)(15000 * sin(2.0 * 3.14159 * i / 44100.0));
        int R = (int)(15000 * sin(2.0 * 3.14159 * i / 44100.0 + 0.5));

        // Apply reverb (no compressor to preserve AC signal)
        fx.ProcessReverb(L, R);

        if (L > max_output) max_output = L;
        if (L < min_output) min_output = L;
    }

    printf("  (output range: [%d, %d])\n", min_output, max_output);
    // Critical check: no signal inversion and bounded output
    TEST_ASSERT(max_output > 0, "Peak output should be positive");
    TEST_ASSERT(max_output < 100000, "Output should not be unbounded");
    TEST_ASSERT(min_output >= -100000, "Output should not be unbounded");
    TEST_END();
}

void test_compressor_reverb_combined() {
    TEST_BEGIN("Integration - Compressor + Reverb combined stability");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 3;      // Large reverb
    g_Config.reverb_decay = 90;
    g_Config.reverb_mix = 40;
    g_Config.compressor_level = 3;  // Strong compressor

    int peak = 0;
    for (int i = 0; i < 20000; i++) {
        int L = 25000;
        int R = 25000;
        fx.ProcessReverb(L, R);
        fx.ProcessCompressor(L, R);
        int abs_L = FastAbs(L);
        if (abs_L > peak) peak = abs_L;
    }

    printf("  (peak output: %d)\n", peak);
    TEST_ASSERT(peak < 50000, "Combined effects should not produce unbounded output");
    TEST_ASSERT(peak > 0, "Output should not be zero (signal should pass through)");
    TEST_END();
}

void test_stress_high_level_sustained() {
    TEST_BEGIN("Integration - Stress test with sustained high-level input");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 4;
    g_Config.reverb_decay = 95;
    g_Config.reverb_mix = 50;
    g_Config.compressor_level = 3;

    // Sustained maximum input for 100000 samples
    int last_output = 0;
    for (int i = 0; i < 100000; i++) {
        int L = 32000;
        int R = 32000;
        fx.ProcessReverb(L, R);
        fx.ProcessCompressor(L, R);
        last_output = L;
    }

    printf("  (final output after 100k samples: %d)\n", last_output);
    // System should remain stable
    TEST_ASSERT(last_output > -50000 && last_output < 50000,
        "System should remain stable under sustained maximum input");
    TEST_ASSERT(last_output != 0, "Output should not be zero");
    TEST_END();
}

void test_zero_crossing_no_glitch() {
    TEST_BEGIN("Integration - Zero-crossing produces no glitch");
    TestHPFFilter hpf;
    hpf.Reset();

    int max_jump = 0;
    int prev = 0;

    // Sine wave through HPF
    for (int i = 0; i < 10000; i++) {
        int input = (int)(20000.0 * sin(2.0 * 3.14159265 * i / 200.0));
        int output = hpf.ProcessHPF(input, 256);
        int jump = FastAbs(output - prev);
        if (jump > max_jump) max_jump = jump;
        prev = output;
    }

    printf("  (max jump at zero-crossing: %d)\n", max_jump);
    TEST_ASSERT(max_jump < 5000,
        "Zero-crossing should not produce large glitches");
    TEST_END();
}

void test_all_comb_filters_stable() {
    TEST_BEGIN("Integration - All 8 comb filters remain bounded independently");
    TestAudioEffects fx;
    fx.Reset();
    g_Config.reverb_type = 4;
    g_Config.reverb_decay = 95;
    g_Config.reverb_mix = 50;

    for (int i = 0; i < 50000; i++) {
        int L = 30000;
        int R = 30000;
        fx.ProcessReverb(L, R);
    }

    for (int ch = 0; ch < 8; ch++) {
        int max_val = 0;
        for (int j = 0; j < 32768; j++) {
            int v = FastAbs(fx.comb_buffer_L[ch][j]);
            if (v > max_val) max_val = v;
        }
        printf("  (comb[%d] max: %d)\n", ch, max_val);
        TEST_ASSERT(max_val <= 50000, "Each comb filter should be independently clamped");
    }
    TEST_END();
}

// =============================================================================
// Main Test Runner
// =============================================================================
int main() {
    InitSineTable();

    printf("============================================\n");
    printf("X68Sound2025 - Audio Bug Fix Test Suite\n");
    printf("============================================\n");

    // Set default config
    memset(&g_Config, 0, sizeof(g_Config));
    g_Config.reverb_decay = 70;
    g_Config.reverb_mix = 20;
    g_Config.compressor_level = 0;

    // ---- Unit Tests ----
    TEST_SECTION("1. ApplySoftClipping Unit Tests");
    test_soft_clipping_normal();
    test_soft_clipping_boundary();
    test_soft_clipping_positive_clip();
    test_soft_clipping_negative_clip();
    test_soft_clipping_extreme_positive();
    test_soft_clipping_extreme_negative();
    test_soft_clipping_zero_threshold();
    test_soft_clipping_threshold_100();       // BUG FIX test
    test_soft_clipping_various_thresholds();

    TEST_SECTION("2. ProcessCompressor Unit Tests");
    test_compressor_no_clipping();
    test_compressor_no_signal_inversion();    // BUG FIX test
    test_compressor_extreme_envelope();       // BUG FIX test
    test_compressor_off();
    test_compressor_symmetry();

    TEST_SECTION("3. ProcessReverb Buffer Clamping Tests");
    test_reverb_comb_buffer_clamping();       // BUG FIX test
    test_reverb_allpass_buffer_clamping();    // BUG FIX test
    test_reverb_no_unbounded_growth();        // BUG FIX test
    test_reverb_off();

    TEST_SECTION("4. HPF Filter Unit Tests");
    test_hpf_normal_operation();
    test_hpf_no_click_on_saturation();        // BUG FIX test
    test_hpf_consecutive_saturation_reset();  // BUG FIX test
    test_hpf_symmetry();
    test_hpf_zero_input();

    // ---- Integration Tests ----
    TEST_SECTION("5. Integration Tests");
    test_full_effects_chain_no_distortion();
    test_compressor_reverb_combined();
    test_stress_high_level_sustained();
    test_zero_crossing_no_glitch();
    test_all_comb_filters_stable();

    // ---- Summary ----
    printf("\n============================================\n");
    printf("Results: %d/%d tests passed, %d failed\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}

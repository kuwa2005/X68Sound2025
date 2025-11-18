class Adpcm {
	int	Scale;		//
	int Pcm;		// 16bit PCM Data
	int	InpPcm,InpPcm_prev,OutPcm;		// 16bit PCM Data for HPF
	int	OutInpPcm,OutInpPcm_prev;		// For HPF
	int	PrevInpPcm;	// Previous InpPcm value for linear interpolation
	volatile int	AdpcmRate;	// 187500(15625*12), 125000(10416.66*12), 93750(7812.5*12), 62500(5208.33*12), 46875(3906.25*12), ...
	int	RateCounter;
	int	N1Data;	// Storage for ADPCM 1 sample data
	int N1DataFlag;	// 0 or 1
	int SaturationCounter;		// Counter for consecutive saturation detection (beep prevention)
	int SaturationCounter62_1;	// Counter for GetPcm62 first stage
	int SaturationCounter62_2;	// Counter for GetPcm62 second stage

	inline void adpcm2pcm(unsigned char adpcm);
	inline void adpcm2pcm_msm6258(unsigned char adpcm);

public:
	void (CALLBACK *IntProc)();	// Interrupt address
	void (CALLBACK *ErrIntProc)();	// Error interrupt address





	inline int	DmaGetByte();
	unsigned char	DmaLastValue;
	volatile unsigned char	AdpcmReg;
	volatile unsigned char	DmaReg[0x40];
int FinishCounter;
	inline void DmaError(unsigned char errcode);
	inline void DmaFinish();
	inline int DmaContinueSetNextMtcMar();
	inline int DmaArrayChainSetNextMtcMar();
	inline int DmaLinkArrayChainSetNextMtcMar();

	Adpcm(void);
	~Adpcm() {};
	inline void	Init();
	inline void	InitSamprate();
	inline void Reset();
	inline int	GetPcm();
	inline int	GetPcm62();

	inline void	SetAdpcmRate(int rate);

};


Adpcm::Adpcm(void) {
	// Initialize saturation detection counters
	SaturationCounter = 0;
	SaturationCounter62_1 = 0;
	SaturationCounter62_2 = 0;
}

inline void Adpcm::SetAdpcmRate(int rate) {
	AdpcmRate = ADPCMRATEADDTBL[rate & 7];
}

const unsigned char DmaRegInit[0x40] = {
/*+00*/	0x00,0x00,	// CSR/CER
/*+02*/	0xFF,0xFF,
/*+04*/	0x80,0x32,	// DCR/OCR
/*+06*/	0x04,0x08,	// SCR/CCR
/*+08*/	0xFF,0xFF,
/*+0A*/	0x00,0x00,	// MTC
/*+0C*/	0x00,0x00,	// MAR
/*+0E*/	0x00,0x00,	// MAR
/*+10*/	0xFF,0xFF,
/*+12*/	0xFF,0xFF,
/*+14*/	0x00,0xE9,	// DAR
/*+16*/	0x20,0x03,	// DAR
/*+18*/	0xFF,0xFF,
/*+1A*/	0x00,0x00,	// BTC
/*+1C*/	0x00,0x00,	// BAR
/*+1E*/	0x00,0x00,	// BAR
/*+20*/	0xFF,0xFF,
/*+22*/	0xFF,0xFF,
/*+24*/	0xFF,0x6A,	// NIV
/*+26*/	0xFF,0x6B,	// EIV
/*+28*/	0xFF,0x05,	// MFC
/*+2A*/	0xFF,0xFF,
/*+2C*/	0xFF,0x01,	// CPR
/*+2E*/	0xFF,0xFF,
/*+30*/	0xFF,0x05,	// DFC
/*+32*/	0xFF,0xFF,
/*+34*/	0xFF,0xFF,
/*+36*/	0xFF,0xFF,
/*+38*/	0xFF,0x05,	// BFC
/*+3A*/	0xFF,0xFF,
/*+3C*/	0xFF,0xFF,
/*+3E*/	0xFF,0x00,	// GCR
};

inline void Adpcm::Init() {
	Scale = 0;
	Pcm = 0;
	InpPcm = InpPcm_prev = OutPcm = 0;
	OutInpPcm = OutInpPcm_prev = 0;
	PrevInpPcm = 0;		// For linear interpolation
	AdpcmRate = 15625*12;
	RateCounter = 0;
	N1Data = 0;
	N1DataFlag = 0;
	IntProc = NULL;
	ErrIntProc = NULL;
	DmaLastValue = 0;
	AdpcmReg = 0xC7;
	{
		int i;
		for (i=0; i<0x40; ++i) {
			DmaReg[i] = DmaRegInit[i];
		}
	}
	FinishCounter = 3;
	DebugLog(1, "[Adpcm::Init] AdpcmReg=0x%02X, AdpcmRate=%d\n", AdpcmReg, AdpcmRate);
}
inline void Adpcm::InitSamprate() {
	RateCounter = 0;
}
inline void Adpcm::Reset() {

	Scale = 0;

	Pcm = 0;
	InpPcm = InpPcm_prev = OutPcm = 0;
	OutInpPcm = OutInpPcm_prev = 0;
	PrevInpPcm = 0;		// For linear interpolation

	N1Data = 0;
	N1DataFlag = 0;

	// Reset RateCounter to prevent inheriting timing state from previous sound
	// This prevents pitch-dependent beep artifacts caused by misaligned sample timing
	RateCounter = 0;

	// Reset saturation counters to prevent false detection from previous playback
	SaturationCounter = 0;
	SaturationCounter62_1 = 0;
	SaturationCounter62_2 = 0;

}


inline void Adpcm::DmaError(unsigned char errcode) {
	// Enhanced DMA error logging
	if (g_Config.debug_log_level >= 2) {
		const char *error_desc = "";
		switch (errcode) {
			case 0x01: error_desc = "Configuration error (invalid CHAIN setting)"; break;
			case 0x09: error_desc = "Bus error (destination address/counter)"; break;
			case 0x0B: error_desc = "Bus error (base address/counter)"; break;
			case 0x0D: error_desc = "Count error (destination address/counter)"; break;
			case 0x11: error_desc = "Software abort error (SAB set while DMA active)"; break;
			default: error_desc = "Unknown error"; break;
		}
		unsigned char *mar = bswapl(*(unsigned char **)&DmaReg[0x0C]);
		unsigned short mtc = bswapw(*(unsigned short *)&DmaReg[0x0A]);
		unsigned char *bar = bswapl(*(unsigned char **)&DmaReg[0x1C]);
		unsigned short btc = bswapw(*(unsigned short *)&DmaReg[0x1A]);
		DebugLog(2, "[ADPCM DMA ERROR] errcode=0x%02X: %s\n", errcode, error_desc);
		DebugLog(2, "  MAR=0x%08X, MTC=%d, BAR=0x%08X, BTC=%d, DmaReg[0x07]=0x%02X\n",
			(unsigned int)(uintptr_t)mar, mtc, (unsigned int)(uintptr_t)bar, btc, DmaReg[0x07]);
	}

	DmaReg[0x00] &= 0xF7;		// ACT=0
	DmaReg[0x00] |= 0x90;		// COC=ERR=1
	DmaReg[0x01] = errcode;		// CER=errorcode
	if (DmaReg[0x07] & 0x08) {	// INT==1?
		if (ErrIntProc != NULL) {
			ErrIntProc();
		}
	}
}
inline void Adpcm::DmaFinish() {
	// Enhanced DMA completion logging
	if (g_Config.debug_log_level >= 3) {
		unsigned char *mar = bswapl(*(unsigned char **)&DmaReg[0x0C]);
		unsigned short mtc = bswapw(*(unsigned short *)&DmaReg[0x0A]);
		DebugLog(3, "[ADPCM DMA FINISH] Transfer complete. MAR=0x%08X, MTC=%d\n",
			(unsigned int)(uintptr_t)mar, mtc);
	}

	DmaReg[0x00] &= 0xF7;		// ACT=0
	DmaReg[0x00] |= 0x80;		// COC=1
	if (DmaReg[0x07] & 0x08) {	// INT==1?
		if (IntProc != NULL) {
			IntProc();
		}
	}
}

inline int Adpcm::DmaContinueSetNextMtcMar() {
	DmaReg[0x07] &= (0xFF-0x40);	// CNT=0
	*(unsigned short *)&DmaReg[0x0A] = *(unsigned short *)&DmaReg[0x1A];	// BTC -> MTC
	*(unsigned int *)&DmaReg[0x0C] = *(unsigned int *)&DmaReg[0x1C];	// BAR -> MAR
	DmaReg[0x29] = DmaReg[0x39];	// BFC -> MFC
	if ( (*(unsigned short *)&DmaReg[0x0A]) == 0 ) {	// MTC == 0 ?
			DmaError(0x0D);	// Count error (destination address/counter)
		return 1;
	}
	DmaReg[0x00] |= 0x40;		// BTC=1
	if (DmaReg[0x07] & 0x08) {	// INT==1?
		if (IntProc != NULL) {
			IntProc();
		}
	}
	return 0;
}
inline int Adpcm::DmaArrayChainSetNextMtcMar() {
	unsigned short	Btc;
	Btc = bswapw(*(unsigned short *)&DmaReg[0x1A]);
	if ( Btc == 0 ) {
		DmaFinish();
		FinishCounter = 0;
		return 1;
	}
	--Btc;
	*(unsigned short *)&DmaReg[0x1A] = bswapw(Btc);

	unsigned char *Bar;
	Bar = bswapl(*(unsigned char **)&DmaReg[0x1C]);
	int	mem0,mem1,mem2,mem3,mem4,mem5;
	mem0 = MemRead(Bar++);
	mem1 = MemRead(Bar++);
	mem2 = MemRead(Bar++);
	mem3 = MemRead(Bar++);
	mem4 = MemRead(Bar++);
	mem5 = MemRead(Bar++);
	if ((mem0|mem1|mem2|mem3|mem4|mem5) == -1) {
		if (g_Config.debug_log_level >= 2) {
			unsigned char *bar_orig = bswapl(*(unsigned char **)&DmaReg[0x1C]);
			unsigned short btc = bswapw(*(unsigned short *)&DmaReg[0x1A]);
			DebugLog(2, "[ADPCM DMA ARRAY CHAIN] MemRead failed in array chain at BAR=0x%08X, BTC=%d\n",
				(unsigned int)(uintptr_t)bar_orig, btc);
		}
			DmaError(0x0B);		// Bus error (base address/counter)
		return 1;
	} 
	*(unsigned char **)&DmaReg[0x1C] = bswapl(Bar);
	DmaReg[0x0C] = mem0;	// MAR
	DmaReg[0x0D] = mem1;
	DmaReg[0x0E] = mem2;
	DmaReg[0x0F] = mem3;
	DmaReg[0x0A] = mem4;	// MTC
	DmaReg[0x0B] = mem5;

	if ( (*(unsigned short *)&DmaReg[0x0A]) == 0 ) {	// MTC == 0 ?
			DmaError(0x0D);	// Count error (destination address/counter)
		return 1;
	}
	return 0;
}
inline int Adpcm::DmaLinkArrayChainSetNextMtcMar() {
	unsigned char *Bar;
	Bar = bswapl(*(unsigned char **)&DmaReg[0x1C]);
	if (Bar == (unsigned char *)0) {
		DmaFinish();
		FinishCounter = 0;
		return 1;
	}

	int	mem0,mem1,mem2,mem3,mem4,mem5;
	int mem6,mem7,mem8,mem9;
	mem0 = MemRead(Bar++);
	mem1 = MemRead(Bar++);
	mem2 = MemRead(Bar++);
	mem3 = MemRead(Bar++);
	mem4 = MemRead(Bar++);
	mem5 = MemRead(Bar++);
	mem6 = MemRead(Bar++);
	mem7 = MemRead(Bar++);
	mem8 = MemRead(Bar++);
	mem9 = MemRead(Bar++);
	if ((mem0|mem1|mem2|mem3|mem4|mem5|mem6|mem7|mem8|mem9) == -1) {
		if (g_Config.debug_log_level >= 2) {
			unsigned char *bar_orig = bswapl(*(unsigned char **)&DmaReg[0x1C]);
			DebugLog(2, "[ADPCM DMA LINK ARRAY CHAIN] MemRead failed in link array chain at BAR=0x%08X\n",
				(unsigned int)(uintptr_t)bar_orig);
		}
			DmaError(0x0B);		// Bus error (base address/counter)
		return 1;
	} 
	*(unsigned char **)&DmaReg[0x1C] = bswapl(Bar);
	DmaReg[0x0C] = mem0;	// MAR
	DmaReg[0x0D] = mem1;
	DmaReg[0x0E] = mem2;
	DmaReg[0x0F] = mem3;
	DmaReg[0x0A] = mem4;	// MTC
	DmaReg[0x0B] = mem5;
	DmaReg[0x1C] = mem6;	// BAR
	DmaReg[0x1D] = mem7;
	DmaReg[0x1E] = mem8;
	DmaReg[0x1F] = mem9;

	if ( (*(unsigned short *)&DmaReg[0x0A]) == 0 ) {	// MTC == 0 ?
			DmaError(0x0D);	// Count error (destination address/counter)
		return 1;
	}
	return 0;
}

int	MACTBL[4] = {0,1,-1,1};

inline int	Adpcm::DmaGetByte() {
	if ((!(DmaReg[0x00]&0x08)) || (DmaReg[0x07]&0x20)) {	// ACT==0 || HLT==1 ?
		return 0x80000000;
	}
	unsigned short	Mtc;
	Mtc = bswapw(*(unsigned short *)&DmaReg[0x0A]);
	if (Mtc == 0) {

//			if (DmaContinueSetNextMtcMar()) {
//				return 0x80000000;
//			}
//			Mtc = bswapw(*(unsigned short *)&DmaReg[0x0A]);
//		} else {
			return 0x80000000;
//		}
	}
	
	
	{
		unsigned char *Mar;
		Mar = bswapl(*(unsigned char **)&DmaReg[0x0C]);
		int mem;
		mem = MemRead(Mar);
		if (mem == -1) {
			if (g_AdpcmDmaErrorCount < 5) {
				DebugLog(3, "[Adpcm::DmaGetByte] MemRead FAILED at address=0x%08X (error_count=%d)\n",
					(unsigned int)(uintptr_t)Mar, g_AdpcmDmaErrorCount);
				g_AdpcmDmaErrorCount++;
			}
			DmaError(0x09);	// Bus error (destination address/counter)
			return 0x80000000;
		}
		DmaLastValue = mem;

		Mar += MACTBL[(DmaReg[0x06]>>2)&3];
		*(unsigned char **)&DmaReg[0x0C] = bswapl(Mar);
	}

	--Mtc;
	*(unsigned short *)&DmaReg[0x0A] = bswapw(Mtc);

	try {
	if (Mtc == 0) {
		// Enhanced logging for DMA chain operations
		if (g_Config.debug_log_level >= 3) {
			DebugLog(3, "[ADPCM DMA CHAIN] MTC reached 0, checking chain mode: DmaReg[0x07]=0x%02X, DmaReg[0x05]=0x%02X\n",
				DmaReg[0x07], DmaReg[0x05]);
		}

		if (DmaReg[0x07] & 0x40) {
			if (g_Config.debug_log_level >= 3) {
				DebugLog(3, "[ADPCM DMA CHAIN] Initiating Continue chain mode\n");
			}
			if (DmaContinueSetNextMtcMar()) {
				throw "";
			}
		} else if (DmaReg[0x05] & 0x08) {
			if (!(DmaReg[0x05] & 0x04)) {
				if (g_Config.debug_log_level >= 3) {
					DebugLog(3, "[ADPCM DMA CHAIN] Initiating Array chain mode\n");
				}
				if (DmaArrayChainSetNextMtcMar()) {
					throw "";
				}
			} else {
				if (g_Config.debug_log_level >= 3) {
					DebugLog(3, "[ADPCM DMA CHAIN] Initiating Link Array chain mode\n");
				}
				if (DmaLinkArrayChainSetNextMtcMar()) {
					throw "";
				}
			}
		} else {
//			if (!(DmaReg[0x00] & 0x40)) {		// BTC=1 ?
//				if (DmaContinueSetNextMtcMar()) {
//					throw "";
//				}
//			} else {
				if (g_Config.debug_log_level >= 3) {
					DebugLog(3, "[ADPCM DMA CHAIN] No chain mode, finishing DMA\n");
				}
				DmaFinish();
				FinishCounter = 0;
//			}
		}
	}
	} catch (void *) {
	}
	
	return DmaLastValue;
}








#define	MAXPCMVAL	(2047)
#define	MAXPCMVAL_MSM6258	(32767)


// MSM6258 / IMA ADPCM high-quality decoder
// -32767<<(4+4) <= InpPcm <= +32767<<(4+4)
inline void	Adpcm::adpcm2pcm_msm6258(unsigned char adpcm) {
	// IMA ADPCM decoding algorithm with higher precision
	int step = dltLTBL_MSM6258[Scale];
	int diff = step >> 3;  // Initialize with step/8

	// Calculate difference using 4-bit ADPCM nibble
	if (adpcm & 4) diff += step;
	if (adpcm & 2) diff += (step >> 1);
	if (adpcm & 1) diff += (step >> 2);

	// Apply sign
	if (adpcm & 8) {
		Pcm -= diff;
	} else {
		Pcm += diff;
	}

	// Clamp to 16-bit range
	if (Pcm > MAXPCMVAL_MSM6258) {
		Pcm = MAXPCMVAL_MSM6258;
	} else if (Pcm < -MAXPCMVAL_MSM6258) {
		Pcm = -MAXPCMVAL_MSM6258;
	}

	// Output with higher resolution (16-bit aligned, shifted for consistency)
	InpPcm = Pcm << (4+4);

	// Update scale index
	Scale += DCT_MSM6258[adpcm];
	if (Scale > 88) {
		Scale = 88;
	} else if (Scale < 0) {
		Scale = 0;
	}
}


// Legacy ADPCM decoder (original X68000)
// -2047<<(4+4) <= InpPcm <= +2047<<(4+4)
inline void	Adpcm::adpcm2pcm(unsigned char adpcm) {
	int	dltL;
	dltL = dltLTBL[Scale];
	dltL = (dltL&(adpcm&4?-1:0)) + ((dltL>>1)&(adpcm&2?-1:0)) + ((dltL>>2)&(adpcm&1?-1:0)) + (dltL>>3);
	int sign = adpcm&8?-1:0;
	dltL = (dltL^sign)+(sign&1);
	Pcm += dltL;

	if ((unsigned int)(Pcm+MAXPCMVAL) > (unsigned int)(MAXPCMVAL*2)) {
		if ((int)(Pcm+MAXPCMVAL) >= (int)(MAXPCMVAL*2)) {
			Pcm = MAXPCMVAL;
		} else {
			Pcm = -MAXPCMVAL;
		}
	}

	InpPcm = (Pcm&(int)0xFFFFFFFC)<<(4+4);

	Scale += DCT[adpcm];
	if ((unsigned int)Scale > (unsigned int)48) {
		if ((int)Scale >= (int)48) {
			Scale = 48;
		} else {
			Scale = 0;
		}
	}
}

// -32768<<4 <= retval <= +32768<<4
inline int Adpcm::GetPcm() {
	if (AdpcmReg & 0x80) {		// ADPCM stop
		return 0x80000000;
	}

	// Linear interpolation: Save previous sample value
	PrevInpPcm = InpPcm;

	RateCounter -= AdpcmRate;
	int needNewSample = (RateCounter < 0);

	while (RateCounter < 0) {
		if (N1DataFlag == 0) {		// Next ADPCM data is required
			int	N10Data;	// (N1Data << 4) | N0Data
			N10Data = DmaGetByte();	// DMA transfer (1 byte)
			if (N10Data == 0x80000000) {
				RateCounter = 0;
				return 0x80000000;
			}
			// Switch decoder based on ADPCM mode
			if (g_Config.adpcm_mode == 1) {
				adpcm2pcm_msm6258(N10Data & 0x0F);	// MSM6258 high-quality decoder
			} else {
				adpcm2pcm(N10Data & 0x0F);	// Legacy decoder
			}
			N1Data = (N10Data >> 4) & 0x0F;
			N1DataFlag = 1;
		} else {
			// Switch decoder based on ADPCM mode
			if (g_Config.adpcm_mode == 1) {
				adpcm2pcm_msm6258(N1Data);	// MSM6258 high-quality decoder
			} else {
				adpcm2pcm(N1Data);	// Legacy decoder
			}
			N1DataFlag = 0;
		}
		RateCounter += 15625*12;
	}

	// Save actual decoded value for HPF filter (before interpolation)
	int InpPcm_for_hpf = InpPcm;

	// Apply linear interpolation (only when new sample is acquired and enabled via environment variable)
	if (g_Config.linear_interpolation && needNewSample) {
		// Interpolate at frac = RateCounter / (15625*12) ratio (16-bit fixed point)
		// Note: RateCounter is now 0 or positive after while loop, so add back AdpcmRate
		int sampleInterval = 15625*12;
		int frac = ((RateCounter + AdpcmRate) << 16) / sampleInterval;
		InpPcm = PrevInpPcm + (((InpPcm - PrevInpPcm) * frac) >> 16);
	}

	// Apply HPF filter (using actual decoded value, not interpolated value)
	// HPF formula: OutPcm = (InpPcm - InpPcm_prev) + feedback_coeff * OutPcm
	int hpf_diff = (InpPcm_for_hpf << HPF_SHIFT) - (InpPcm_prev << HPF_SHIFT);
	int hpf_feedback = HPF_COEFF_A1_22KHZ * OutPcm;
	OutPcm = (hpf_diff + hpf_feedback) >> HPF_SHIFT;
	InpPcm_prev = InpPcm_for_hpf;  // Save actual decoded value for next iteration

	// Prevent HPF filter oscillation by resetting filter state when output saturates
	// When OutPcm reaches saturation level (±70000), the feedback term becomes huge
	// (459 * 70000 / 512 = 62,695), causing permanent oscillation and beep artifacts.
	// Solution: Detect consecutive saturation and aggressively reset to break feedback loop.
	const int OUT_PCM_LIMIT = 70000;
	const int OUT_PCM_RESET_THRESHOLD = 60000;  // Lowered from 65000 to catch oscillation earlier
	const int OUT_PCM_HARD_THRESHOLD = 50000;   // If consecutive saturation occurs, use stricter limit

	// Check if filter output is approaching saturation
	int abs_OutPcm = (OutPcm >= 0) ? OutPcm : -OutPcm;
	if (abs_OutPcm > OUT_PCM_RESET_THRESHOLD) {
		SaturationCounter++;

		// If consecutive saturation detected (2+ samples in a row), aggressively reset
		if (SaturationCounter >= 2 || abs_OutPcm > OUT_PCM_HARD_THRESHOLD) {
			// Complete filter state reset to break oscillation
			OutPcm = 0;           // Reset to zero (most aggressive)
			InpPcm_prev = 0;      // Also reset filter history
			SaturationCounter = 0; // Reset counter after intervention
		} else {
			// Single saturation - gentle reset to input value
			OutPcm = InpPcm_for_hpf >> HPF_SHIFT;
			if (OutPcm > OUT_PCM_LIMIT) OutPcm = OUT_PCM_LIMIT;
			if (OutPcm < -OUT_PCM_LIMIT) OutPcm = -OUT_PCM_LIMIT;
		}
	} else {
		// Normal operation - clear saturation counter
		SaturationCounter = 0;
	}

	int result = (OutPcm*TotalVolume)>>8;

	// Saturate final result to prevent clipping and beep artifacts in downstream mixer
	const int RESULT_LIMIT = 40000;
	if (result > RESULT_LIMIT) {
		result = RESULT_LIMIT;
	} else if (result < -RESULT_LIMIT) {
		result = -RESULT_LIMIT;
	}

	return result;
}

// -32768<<4 <= retval <= +32768<<4
inline int Adpcm::GetPcm62() {
	if (AdpcmReg & 0x80) {		// ADPCM stop
		return 0x80000000;
	}

	// Linear interpolation: Save previous sample value
	PrevInpPcm = InpPcm;

	RateCounter -= AdpcmRate;
	int needNewSample = (RateCounter < 0);

	while (RateCounter < 0) {
		if (N1DataFlag == 0) {		// Next ADPCM data is required
			int	N10Data;	// (N1Data << 4) | N0Data
			N10Data = DmaGetByte();	// DMA transfer (1 byte)
			if (N10Data == 0x80000000) {
				RateCounter = 0;
				return 0x80000000;
			}
			// Switch decoder based on ADPCM mode
			if (g_Config.adpcm_mode == 1) {
				adpcm2pcm_msm6258(N10Data & 0x0F);	// MSM6258 high-quality decoder
			} else {
				adpcm2pcm(N10Data & 0x0F);	// Legacy decoder
			}
			N1Data = (N10Data >> 4) & 0x0F;
			N1DataFlag = 1;
		} else {
			// Switch decoder based on ADPCM mode
			if (g_Config.adpcm_mode == 1) {
				adpcm2pcm_msm6258(N1Data);	// MSM6258 high-quality decoder
			} else {
				adpcm2pcm(N1Data);	// Legacy decoder
			}
			N1DataFlag = 0;
		}
		RateCounter += 15625*12*4;

	}

	// Save actual decoded value for HPF filter (before interpolation)
	int InpPcm_for_hpf = InpPcm;

	// Apply linear interpolation (only when new sample is acquired and enabled via environment variable)
	if (g_Config.linear_interpolation && needNewSample) {
		// Interpolate at frac = RateCounter / (15625*12*4) ratio (16-bit fixed point)
		// Note: RateCounter is now 0 or positive after while loop, so add back AdpcmRate
		int sampleInterval = 15625*12*4;
		int frac = ((RateCounter + AdpcmRate) << 16) / sampleInterval;
		InpPcm = PrevInpPcm + (((InpPcm - PrevInpPcm) * frac) >> 16);
	}

	// Apply HPF filter (using actual decoded value, not interpolated value)
	// Two-stage cascade filter for better DC offset removal
	OutInpPcm = (InpPcm_for_hpf<<9) - (InpPcm_prev<<9) +  OutInpPcm-(OutInpPcm>>5)-(OutInpPcm>>10);
	InpPcm_prev = InpPcm_for_hpf;  // Save actual decoded value for next iteration

	// Prevent first stage oscillation by detecting consecutive saturation
	const int OUT_INP_PCM_LIMIT = 30000000;  // ~58000 after >>9 shift
	const int OUT_INP_PCM_RESET_THRESHOLD = 24000000;  // Lowered from 27M for earlier detection
	const int OUT_INP_PCM_HARD_THRESHOLD = 20000000;

	int abs_OutInpPcm = (OutInpPcm >= 0) ? OutInpPcm : -OutInpPcm;
	if (abs_OutInpPcm > OUT_INP_PCM_RESET_THRESHOLD) {
		SaturationCounter62_1++;

		if (SaturationCounter62_1 >= 2 || abs_OutInpPcm > OUT_INP_PCM_HARD_THRESHOLD) {
			// Consecutive saturation - aggressive reset
			OutInpPcm = 0;
			InpPcm_prev = 0;
			SaturationCounter62_1 = 0;
		} else {
			// Single saturation - gentle reset
			OutInpPcm = (InpPcm_for_hpf<<9);
			if (OutInpPcm > OUT_INP_PCM_LIMIT) OutInpPcm = OUT_INP_PCM_LIMIT;
			if (OutInpPcm < -OUT_INP_PCM_LIMIT) OutInpPcm = -OUT_INP_PCM_LIMIT;
		}
	} else {
		SaturationCounter62_1 = 0;
	}

	OutPcm = OutInpPcm - OutInpPcm_prev + OutPcm-(OutPcm>>8)-(OutPcm>>9)-(OutPcm>>12);
	OutInpPcm_prev = OutInpPcm;

	// Prevent second stage oscillation by detecting consecutive saturation
	const int OUT_PCM_LIMIT_62 = 50000000;  // ~97000 after >>9 shift
	const int OUT_PCM_RESET_THRESHOLD_62 = 40000000;  // Lowered from 45M for earlier detection
	const int OUT_PCM_HARD_THRESHOLD_62 = 35000000;

	int abs_OutPcm_62 = (OutPcm >= 0) ? OutPcm : -OutPcm;
	if (abs_OutPcm_62 > OUT_PCM_RESET_THRESHOLD_62) {
		SaturationCounter62_2++;

		if (SaturationCounter62_2 >= 2 || abs_OutPcm_62 > OUT_PCM_HARD_THRESHOLD_62) {
			// Consecutive saturation - aggressive reset
			OutPcm = 0;
			OutInpPcm_prev = 0;
			SaturationCounter62_2 = 0;
		} else {
			// Single saturation - gentle reset to first stage output
			OutPcm = OutInpPcm;
			if (OutPcm > OUT_PCM_LIMIT_62) OutPcm = OUT_PCM_LIMIT_62;
			if (OutPcm < -OUT_PCM_LIMIT_62) OutPcm = -OUT_PCM_LIMIT_62;
		}
	} else {
		SaturationCounter62_2 = 0;
	}

	int result = ((OutPcm>>9)*TotalVolume)>>8;

	// Saturate final result to prevent clipping and beep artifacts in downstream mixer
	const int RESULT_LIMIT = 40000;
	if (result > RESULT_LIMIT) {
		result = RESULT_LIMIT;
	} else if (result < -RESULT_LIMIT) {
		result = -RESULT_LIMIT;
	}

	return result;
}


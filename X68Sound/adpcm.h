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

	inline void adpcm2pcm(unsigned char adpcm);

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
	DebugLog("[Adpcm::Init] AdpcmReg=0x%02X, AdpcmRate=%d\n", AdpcmReg, AdpcmRate);
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


}


inline void Adpcm::DmaError(unsigned char errcode) {
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
				DebugLog("[Adpcm::DmaGetByte] MemRead FAILED at address=0x%08X (error_count=%d)\n",
					(unsigned int)(uintptr_t)Mar, g_AdpcmDmaErrorCount);
				g_AdpcmDmaErrorCount++;
			}
			DmaError(0x09);	// Bus error (destination address/counter)
			return 0x80000000;
		}
		DmaLastValue = mem;

		if (g_AdpcmDmaReadCount < 50) {
			DebugLog("[Adpcm::DmaGetByte] MemRead SUCCESS at address=0x%08X, data=0x%02X (read_count=%d)\n",
				(unsigned int)(uintptr_t)Mar, mem, g_AdpcmDmaReadCount);
			g_AdpcmDmaReadCount++;
		}

		Mar += MACTBL[(DmaReg[0x06]>>2)&3];
		*(unsigned char **)&DmaReg[0x0C] = bswapl(Mar);
	}

	--Mtc;
	*(unsigned short *)&DmaReg[0x0A] = bswapw(Mtc);

	try {
	if (Mtc == 0) {
		if (DmaReg[0x07] & 0x40) {
			if (DmaContinueSetNextMtcMar()) {
				throw "";
			}
		} else if (DmaReg[0x05] & 0x08) {
			if (!(DmaReg[0x05] & 0x04)) {
				if (DmaArrayChainSetNextMtcMar()) {
					throw "";
				}
			} else {
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
	int logThis = (g_Adpcm2PcmCallCount < 30);
	int oldPcm = Pcm;
	int oldScale = Scale;

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

	if (logThis) {
		DebugLog("[adpcm2pcm_msm6258] adpcm=0x%02X, diff=%d, Scale: %d->%d, Pcm: %d->%d, InpPcm=%d (count=%d)\n",
			adpcm, diff, oldScale, Scale, oldPcm, Pcm, InpPcm, g_Adpcm2PcmCallCount);
		g_Adpcm2PcmCallCount++;
	}
}


// Legacy ADPCM decoder (original X68000)
// -2047<<(4+4) <= InpPcm <= +2047<<(4+4)
inline void	Adpcm::adpcm2pcm(unsigned char adpcm) {

	int logThis = (g_Adpcm2PcmCallCount < 30);
	int oldPcm = Pcm;
	int oldScale = Scale;

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

	if (logThis) {
		DebugLog("[adpcm2pcm] adpcm=0x%02X, dltL=%d, Scale: %d->%d, Pcm: %d->%d, InpPcm=%d (count=%d)\n",
			adpcm, dltL, oldScale, Scale, oldPcm, Pcm, InpPcm, g_Adpcm2PcmCallCount);
		g_Adpcm2PcmCallCount++;
	}
}

// -32768<<4 <= retval <= +32768<<4
inline int Adpcm::GetPcm() {
	int logThis = (g_AdpcmGetPcmCallCount < 20);
	if (logThis) {
		DebugLog("[Adpcm::GetPcm] called, AdpcmReg=0x%02X (call_count=%d)\n", AdpcmReg, g_AdpcmGetPcmCallCount);
		g_AdpcmGetPcmCallCount++;
	}

	if (AdpcmReg & 0x80) {		// ADPCM stop
		if (logThis) {
			DebugLog("[Adpcm::GetPcm] STOPPED, returning 0x80000000\n");
		}
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

	// Apply linear interpolation (only when new sample is acquired and enabled via environment variable)
	if (g_Config.linear_interpolation && needNewSample) {
		// Interpolate at frac = RateCounter / (15625*12) ratio (16-bit fixed point)
		// Note: RateCounter is now 0 or positive after while loop, so add back AdpcmRate
		int sampleInterval = 15625*12;
		int frac = ((RateCounter + AdpcmRate) << 16) / sampleInterval;
		InpPcm = PrevInpPcm + (((InpPcm - PrevInpPcm) * frac) >> 16);
	}

	// Apply HPF filter (magic numbers replaced with constants)
	OutPcm = ((InpPcm << HPF_SHIFT) - (InpPcm_prev << HPF_SHIFT) + HPF_COEFF_A1_22KHZ * OutPcm) >> HPF_SHIFT;
	InpPcm_prev = InpPcm;

	int result = (OutPcm*TotalVolume)>>8;
	if (logThis) {
		DebugLog("[Adpcm::GetPcm] PLAYING, OutPcm=%d, TotalVolume=%d, result=%d\n", OutPcm, TotalVolume, result);
	}
	return result;
}

// -32768<<4 <= retval <= +32768<<4
inline int Adpcm::GetPcm62() {
	int logThis = (g_AdpcmGetPcm62CallCount < 20);
	if (logThis) {
		DebugLog("[Adpcm::GetPcm62] called, AdpcmReg=0x%02X (call_count=%d)\n", AdpcmReg, g_AdpcmGetPcm62CallCount);
		g_AdpcmGetPcm62CallCount++;
	}

	if (AdpcmReg & 0x80) {		// ADPCM stop
		if (logThis) {
			DebugLog("[Adpcm::GetPcm62] STOPPED, returning 0x80000000\n");
		}
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

	// Apply linear interpolation (only when new sample is acquired and enabled via environment variable)
	if (g_Config.linear_interpolation && needNewSample) {
		// Interpolate at frac = RateCounter / (15625*12*4) ratio (16-bit fixed point)
		// Note: RateCounter is now 0 or positive after while loop, so add back AdpcmRate
		int sampleInterval = 15625*12*4;
		int frac = ((RateCounter + AdpcmRate) << 16) / sampleInterval;
		InpPcm = PrevInpPcm + (((InpPcm - PrevInpPcm) * frac) >> 16);
	}

	OutInpPcm = (InpPcm<<9) - (InpPcm_prev<<9) +  OutInpPcm-(OutInpPcm>>5)-(OutInpPcm>>10);
	InpPcm_prev = InpPcm;
	OutPcm = OutInpPcm - OutInpPcm_prev + OutPcm-(OutPcm>>8)-(OutPcm>>9)-(OutPcm>>12);
	OutInpPcm_prev = OutInpPcm;

	int result = ((OutPcm>>9)*TotalVolume)>>8;
	if (logThis) {
		DebugLog("[Adpcm::GetPcm62] PLAYING, OutPcm=%d, TotalVolume=%d, result=%d\n", OutPcm, TotalVolume, result);
	}
	return result;
}


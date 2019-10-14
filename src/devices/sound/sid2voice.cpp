// license:BSD-3-Clause
// copyright-holders:Peter Trauner
#include "emu.h"
#include "sid2voice.h"

#include "sid2.h"
#include "sid2envel.h"
#include "sidw6581.h"
#include "sidw8580.h"

#include "sound/jkm8580.h"


std::unique_ptr<float[]> filterTable;
std::unique_ptr<float[]> bandPassParam;
#define lowPassParam filterTable
float filterResTable[16];

static uint8_t triangleTable[4096];
static uint8_t sawtoothTable[4096];
static uint8_t squareTable[2*4096];
static const uint8_t* waveform30[2];
static const uint8_t* waveform50[2];
static const uint8_t* waveform60[2];
static const uint8_t* waveform70[2];
#if defined(LARGE_NOISE_TABLE)
	static uint8_t noiseTableMSB[1<<8];
	static uint8_t noiseTableLSB[1L<<16];
#else
	static uint8_t noiseTableMSB[1<<8];
	static uint8_t noiseTableMID[1<<8];
	static uint8_t noiseTableLSB[1<<8];
#endif

void filterTableInit(running_machine &machine)
{
	int sample_rate = machine.sample_rate();
	uint16_t uk;
	/* Parameter calculation has not been moved to a separate function */
	/* by purpose. */
	const float filterRefFreq = 44100.0f;

	float yMax = 1.0f;
	float yMin = 0.01f;
	float yAdd;
	float yTmp, rk, rk2;

	float resDyMax;
	float resDyMin;
	float resDy;

	filterTable = std::make_unique<float[]>(0x800);
	bandPassParam = std::make_unique<float[]>(0x800);

	uk = 0;
	for (rk = 0; rk < 0x800; rk++)
	{
		filterTable[uk] = (((expf(rk/0x800*logf(400.0f))/60.0f)+0.05f)
			*filterRefFreq) / sample_rate;
		if (filterTable[uk] < yMin)
			filterTable[uk] = yMin;
		if (filterTable[uk] > yMax)
			filterTable[uk] = yMax;
		uk++;
	}

	/*extern float bandPassParam[0x800]; */
	yMax = 0.22f;
	yMin = 0.05f;  /* less for some R1/R4 chips */
	yAdd = (yMax-yMin)/2048.0f;
	yTmp = yMin;
	uk = 0;
	/* Some C++ compilers still have non-local scope! */
	for (rk2 = 0; rk2 < 0x800; rk2++)
	{
		bandPassParam[uk] = (yTmp*filterRefFreq) / sample_rate;
		yTmp += yAdd;
		uk++;
	}

	/*extern float filterResTable[16]; */
	resDyMax = 1.0f;
	resDyMin = 2.0f;
	resDy = resDyMin;
	for (uk = 0; uk < 16; uk++)
	{
		filterResTable[uk] = resDy;
		resDy -= ((resDyMin - resDyMax) / 15);
	}
	filterResTable[0] = resDyMin;
	filterResTable[15] = resDyMax;
}


static std::unique_ptr<int8_t[]> ampMod1x8;

static const uint32_t noiseSeed = 0x7ffff8;

void sidInitMixerEngine(running_machine &machine)
{
	/* 8-bit volume modulation tables. */
	float filterAmpl = 0.7f;

	ampMod1x8 = std::make_unique<int8_t[]>(256*256);

	uint16_t uk = 0;
	for (int32_t si = 0; si < 256; si++)
	{
		for (int32_t sj = -128; sj < 128; sj++, uk++)
		{
			ampMod1x8[uk] = (int8_t)(((si*sj)/255)*filterAmpl);
		}
	}

}

static inline void waveAdvance(jkm8580Operator* pVoice)
{
	pVoice->reg[14] = pVoice->output;
	pVoice->reg[15] = pVoice->enveVol;
#if defined(DIRECT_FIXPOINT)
	pVoice->waveStep.l += pVoice->waveStepAdd.l;
	pVoice->waveStep.w.h &= 4095;
#else
	pVoice->waveStepPnt += pVoice->waveStepAddPnt;
	pVoice->waveStep += pVoice->waveStepAdd;
	if (pVoice->waveStepPnt > 65535) pVoice->waveStep++;
	pVoice->waveStepPnt &= 0xFFFF;
	pVoice->waveStep &= 4095;
#endif
}

static inline void noiseAdvance(jkm8580Operator* pVoice)
{
	pVoice->noiseStep += pVoice->noiseStepAdd;
	if (pVoice->noiseStep >= (1L << 20))
	{
		pVoice->noiseStep -= (1L << 20);
#if defined(DIRECT_FIXPOINT)
		pVoice->noiseReg.l = (pVoice->noiseReg.l << 1) |
			(((pVoice->noiseReg.l >> 22) ^ (pVoice->noiseReg.l >> 17)) & 1);
#else
		pVoice->noiseReg = (pVoice->noiseReg << 1) |
			(((pVoice->noiseReg >> 22) ^ (pVoice->noiseReg >> 17)) & 1);
#endif
#if defined(DIRECT_FIXPOINT) && defined(LARGE_NOISE_TABLE)
		pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.w.l]
								|noiseTableMSB[pVoice->noiseReg.w.h & 0xff]);
#elif defined(DIRECT_FIXPOINT)
		pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.b.l]
								|noiseTableMID[pVoice->noiseReg.b.h]
								|noiseTableMSB[pVoice->noiseReg.b.h2]);
#else
		pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg & 0xff]
								|noiseTableMID[pVoice->noiseReg >> 8 & 0xff]
								|noiseTableMSB[pVoice->noiseReg >> 16 & 0xff]);
#endif
	}
}

static inline void noiseAdvanceHp(jkm8580Operator* pVoice)
{
	uint32_t tmp = pVoice->noiseStepAdd;
	while (tmp >= (1L << 20))
	{
		tmp -= (1L << 20);
#if defined(DIRECT_FIXPOINT)
		pVoice->noiseReg.l = (pVoice->noiseReg.l << 1) |
			(((pVoice->noiseReg.l >> 22) ^ (pVoice->noiseReg.l >> 17)) & 1);
#else
		pVoice->noiseReg = (pVoice->noiseReg << 1) |
			(((pVoice->noiseReg >> 22) ^ (pVoice->noiseReg >> 17)) & 1);
#endif
	}
	pVoice->noiseStep += tmp;
	if (pVoice->noiseStep >= (1L << 20))
	{
		pVoice->noiseStep -= (1L << 20);
#if defined(DIRECT_FIXPOINT)
		pVoice->noiseReg.l = (pVoice->noiseReg.l << 1) |
			(((pVoice->noiseReg.l >> 22) ^ (pVoice->noiseReg.l >> 17)) & 1);
#else
		pVoice->noiseReg = (pVoice->noiseReg << 1) |
			(((pVoice->noiseReg >> 22) ^ (pVoice->noiseReg >> 17)) & 1);
#endif
	}
#if defined(DIRECT_FIXPOINT) && defined(LARGE_NOISE_TABLE)
	pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.w.l]
							|noiseTableMSB[pVoice->noiseReg.w.h & 0xff]);
#elif defined(DIRECT_FIXPOINT)
	pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.b.l]
							|noiseTableMID[pVoice->noiseReg.b.h]
							|noiseTableMSB[pVoice->noiseReg.b.h2]);
#else
	pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg & 0xff]
							|noiseTableMID[pVoice->noiseReg >> 8 & 0xff]
							|noiseTableMSB[pVoice->noiseReg >> 16 & 0xff]);
#endif
}


#if defined(DIRECT_FIXPOINT)
	#define triangle triangleTable[pVoice->waveStep.w.h]
	#define sawtooth sawtoothTable[pVoice->waveStep.w.h]
	#define square squareTable[pVoice->waveStep.w.h + pVoice->pulseIndex]
	#define triSaw waveform30[pVoice->sidwavemode][pVoice->waveStep.w.h]
	#define triSquare waveform50[pVoice->sidwavemode][pVoice->waveStep.w.h + pVoice->SIDpulseWidth]
	#define sawSquare waveform60[pVoice->sidwavemode][pVoice->waveStep.w.h + pVoice->SIDpulseWidth]
	#define triSawSquare waveform70[pVoice->sidwavemode][pVoice->waveStep.w.h + pVoice->SIDpulseWidth]
#else
	#define triangle triangleTable[pVoice->waveStep]
	#define sawtooth sawtoothTable[pVoice->waveStep]
	#define square squareTable[pVoice->waveStep + pVoice->pulseIndex]
	#define triSaw waveform30[pVoice->sidwavemode][pVoice->waveStep]
	#define triSquare waveform50[pVoice->sidwavemode][pVoice->waveStep + pVoice->SIDpulseWidth]
	#define sawSquare waveform60[pVoice->sidwavemode][pVoice->waveStep + pVoice->SIDpulseWidth]
	#define triSawSquare waveform70[pVoice->sidwavemode][pVoice->waveStep + pVoice->SIDpulseWidth]
#endif


static void sidMode00(jkm8580Operator* pVoice)
{
	pVoice->output = (pVoice->filtIO - 0x80);
	waveAdvance(pVoice);
}

static void sidModeC0(jkm8580Operator* pVoice)
{
	pVoice->output = (pVoice->masterVolume << 4);
	waveAdvance(pVoice);
}

#if 0
/* not used */
static void sidModeReal00(jkm8580Operator* pVoice)
{
	pVoice->output = 0;
	waveAdvance(pVoice);
}
#endif

static void sidMode10(jkm8580Operator* pVoice)
{
	pVoice->output = triangle;
	waveAdvance(pVoice);
}

static void sidMode20(jkm8580Operator* pVoice)
{
	pVoice->output = sawtooth;
	waveAdvance(pVoice);
}

static void sidMode30(jkm8580Operator* pVoice)
{
	pVoice->output = triSaw;
	waveAdvance(pVoice);
}

static void sidMode40(jkm8580Operator* pVoice)
{
	pVoice->output = square;
	waveAdvance(pVoice);
}

static void sidMode50(jkm8580Operator* pVoice)
{
	pVoice->output = triSquare;
	waveAdvance(pVoice);
}

static void sidMode60(jkm8580Operator* pVoice)
{
	pVoice->output = sawSquare;
	waveAdvance(pVoice);
}

static void sidMode70(jkm8580Operator* pVoice)
{
	pVoice->output = triSawSquare;
	waveAdvance(pVoice);
}

static void sidMode80(jkm8580Operator* pVoice)
{
	pVoice->output = pVoice->noiseOutput;
	waveAdvance(pVoice);
	noiseAdvance(pVoice);
}

static void sidMode80hp(jkm8580Operator* pVoice)
{
	pVoice->output = pVoice->noiseOutput;
	waveAdvance(pVoice);
	noiseAdvanceHp(pVoice);
}

static void sidModeLock(jkm8580Operator* pVoice)
{
	pVoice->noiseIsLocked = true;
	pVoice->output = (pVoice->filtIO - 0x80);
	waveAdvance(pVoice);
}

/* */
/* */
/* */

static void sidModeC4(jkm8580Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if (pVoice->modulator->waveStep.w.h < 2048)
#else
	if (pVoice->modulator->waveStep < 2048)
#endif
	pVoice->output = (pVoice->masterVolume << 4);
	else
	pVoice->output = 0xFF ^ (pVoice->masterVolume << 4);
	waveAdvance(pVoice);
}

static void sidMode14(jkm8580Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if (pVoice->modulator->waveStep.w.h < 2048)
#else
	if (pVoice->modulator->waveStep < 2048)
#endif
	pVoice->output = triangle;
	else
	pVoice->output = 0xFF ^ triangle;
	waveAdvance(pVoice);
}

static void sidMode34(jkm8580Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if (pVoice->modulator->waveStep.w.h < 2048)
#else
	if (pVoice->modulator->waveStep < 2048)
#endif
	pVoice->output = triSaw;
	else
	pVoice->output = 0xFF ^ triSaw;
	waveAdvance(pVoice);
}

static void sidMode54(jkm8580Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if (pVoice->modulator->waveStep.w.h < 2048)
#else
	if (pVoice->modulator->waveStep < 2048)
#endif
	pVoice->output = triSquare;
	else
	pVoice->output = 0xFF ^ triSquare;
	waveAdvance(pVoice);
}

static void sidMode74(jkm8580Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if (pVoice->modulator->waveStep.w.h < 2048)
#else
	if (pVoice->modulator->waveStep < 2048)
#endif
	pVoice->output = triSawSquare;
	else
	pVoice->output = 0xFF ^ triSawSquare;
	waveAdvance(pVoice);
}

/* */
/* */
/* */

static inline void waveCalcFilter(jkm8580Operator* pVoice)
{
	if (pVoice->filtEnabled)
	{
		if (pVoice->filter.Type != 0)
		{
			if (pVoice->filter.Type == 0x20)
			{
				float tmp;
				pVoice->filtLow += (pVoice->filtRef * pVoice->filter.Dy);
				tmp = (float)pVoice->filtIO - pVoice->filtLow;
				tmp -= pVoice->filtRef * pVoice->filter.ResDy;
				pVoice->filtRef += (tmp * (pVoice->filter.Dy));
				pVoice->filtIO = (int8_t)(pVoice->filtRef-pVoice->filtLow/4);
			}
			else if (pVoice->filter.Type == 0x40)
			{
				float tmp, tmp2;
				pVoice->filtLow += (pVoice->filtRef * pVoice->filter.Dy * 0.1f);
				tmp = (float)pVoice->filtIO - pVoice->filtLow;
				tmp -= pVoice->filtRef * pVoice->filter.ResDy;
				pVoice->filtRef += (tmp * (pVoice->filter.Dy));
				tmp2 = pVoice->filtRef - pVoice->filtIO/8;
				if (tmp2 < -128)
					tmp2 = -128;
				if (tmp2 > 127)
					tmp2 = 127;
				pVoice->filtIO = (int8_t)tmp2;
			}
			else
			{
				float sample, sample2;
				int tmp;
				pVoice->filtLow += (pVoice->filtRef * pVoice->filter.Dy);
				sample = pVoice->filtIO;
				sample2 = sample - pVoice->filtLow;
				tmp = (int)sample2;
				sample2 -= pVoice->filtRef * pVoice->filter.ResDy;
				pVoice->filtRef += (sample2 * pVoice->filter.Dy);

				if (pVoice->filter.Type == 0x10)
				{
					pVoice->filtIO = (int8_t)pVoice->filtLow;
				}
				else if (pVoice->filter.Type == 0x30)
				{
					pVoice->filtIO = (int8_t)pVoice->filtLow;
				}
				else if (pVoice->filter.Type == 0x50)
				{
					pVoice->filtIO = (int8_t)(sample - (tmp >> 1));
				}
				else if (pVoice->filter.Type == 0x60)
				{
					pVoice->filtIO = (int8_t)tmp;
				}
				else if (pVoice->filter.Type == 0x70)
				{
					pVoice->filtIO = (int8_t)(sample - (tmp >> 1));
				}
			}
		}
		else /* pVoice->filter.Type == 0x00 */
		{
			pVoice->filtIO = 0;
		}
	}
}

static int8_t waveCalcMute(jkm8580Operator* pVoice)
{
	(*pVoice->ADSRproc)(pVoice);  /* just process envelope */
	return pVoice->filtIO;
}


static int8_t waveCalcRangeCheck(jkm8580Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	pVoice->waveStepOld = pVoice->waveStep.w.h;
	(*pVoice->waveProc)(pVoice);
	if (pVoice->waveStep.w.h < pVoice->waveStepOld)
#else
	pVoice->waveStepOld = pVoice->waveStep;
	(*pVoice->waveProc)(pVoice);
	if (pVoice->waveStep < pVoice->waveStepOld)
#endif
	{
		/* Next step switch back to normal calculation. */
		pVoice->cycleLenCount = 0;
		pVoice->outProc = &jkm8580Operator::wave_calc_normal;
#if defined(DIRECT_FIXPOINT)
				pVoice->waveStep.w.h = 4095;
#else
				pVoice->waveStep = 4095;
#endif
	}
	pVoice->filtIO = ampMod1x8[(*pVoice->ADSRproc)(pVoice)|pVoice->output];
	waveCalcFilter(pVoice);
	return pVoice->filtIO;
}

/* MOS-8580, MOS-6581 (no 70) */
static ptr2sidVoidFunc sidModeNormalTable[2][16] =
{
	{
		sidMode00, sidMode10, sidMode20, sidMode30, sidMode40, sidMode50, sidMode60, sidMode70,
		sidMode80, sidModeLock, sidModeLock, sidModeLock, sidModeC0, sidModeC0, sidModeC0, sidModeC0
	},
	{
		sidMode00, sidMode10, sidMode20, sidMode30, sidMode40, sidMode50, sidMode60, sidMode70,
		sidMode80, sidModeLock, sidModeLock, sidModeLock, sidModeC0, sidModeC0, sidModeC0, sidModeC0
	}
};

/* MOS-8580, MOS-6581 (no 74) */
static ptr2sidVoidFunc sidModeRingTable[2][16] =
{
	{
		sidMode00, sidMode14, sidMode00, sidMode34, sidMode00, sidMode54, sidMode00, sidMode74,
		sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeC4, sidModeC4, sidModeC4, sidModeC4
	},
	{
		sidMode00, sidMode14, sidMode00, sidMode34, sidMode00, sidMode54, sidMode00, sidMode74,
		sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeC4, sidModeC4, sidModeC4, sidModeC4
	}
};


void jkm8580Operator::clear()
{
	sidwavemode = 0;
	sidprocessmode = 0;
	SIDfreq = 0;
	SIDctrl = 0;
	SIDAD = 0;
	SIDSR = 0;

	sync = false;

	pulseIndex = newPulseIndex = SIDpulseWidth = 0;
	curSIDfreq = curNoiseFreq = 0;

	output = noiseOutput = 0;
	outputLMask = ~0;
	outputRMask = ~0;
	filtIO = 0;

	filter.Enabled = true;

	filter.Type = filter.CurType = 0;
	filter.Value = 0;
	filter.Dy = filter.ResDy = 0;

	filtEnabled = false;
	filtLow = filtRef = 0;

	cycleLenCount = 0;
#if defined(DIRECT_FIXPOINT)
	cycleLen.l = cycleAddLen.l = 0;
#else
	cycleLen = cycleLenPnt = 0;
	cycleAddLenPnt = 0;
#endif

	outProc = waveCalcMute;

#if defined(DIRECT_FIXPOINT)
	waveStepAdd.l = waveStep.l = 0;
	wavePre[0].len = (wavePre[0].stp = 0);
	wavePre[1].len = (wavePre[1].stp = 0);
#else
	waveStepAdd = waveStepAddPnt = 0;
	waveStep = waveStepPnt = 0;
	wavePre[0].len = 0;
	wavePre[0].stp = wavePre[0].pnt = 0;
	wavePre[1].len = 0;
	wavePre[1].stp = wavePre[1].pnt = 0;
#endif
	waveStepOld = 0;

#if defined(DIRECT_FIXPOINT)
	noiseReg.l = noiseSeed;
#else
	noiseReg = noiseSeed;
#endif
	noiseStepAdd = noiseStep = 0;
	noiseIsLocked = false;
}


/* -------------------------------------------------- Operator frame set-up 1 */

void jkm8580Operator::set()
{
	sidwavemode = reg[11] & 1;
	sidprocessmode = (reg[11] & 2) >> 1;
	// filter mode (reg[11] & 4) >> 2;

	masterVolume = reg[10] & 15;
	masterVolumeAmplIndex = masterVolume << 8;

	if ((reg[10] & 0x80) && !(reg[9] & 1))
		outputLMask = 0;     /* off */
	else
		outputLMask = ~0;  /* on */

	if ((reg[10] & 0x80) && !(reg[9] & 2))
		outputRMask = 0;     /* off */
	else
		outputRMask = ~0;  /* on */

	filter.Type = reg[10] & 0x70;
	if (filter.Type != filter.CurType)
	{
		filter.CurType = filter.Type;
		filtLow = filtRef = 0;
	}
	if (filter.Enabled)
	{
		filter.Value = 0x7ff & ((reg[7] & 7) | (uint16_t(reg[8]) << 3));
		if (filter.Type == 0x20)
			filter.Dy = bandPassParam ? bandPassParam[filter.Value] : 0.0f;
		else
			filter.Dy = lowPassParam ? lowPassParam[filter.Value] : 0.0f;
		filter.ResDy = filterResTable[reg[9] >> 4] - filter.Dy;
		if (filter.ResDy < 1.0f)
			filter.ResDy = 1.0f;
	}

	SIDfreq = reg[0] | (reg[1] << 8);

	SIDpulseWidth = (reg[2] | (reg[3] << 8)) & 0x0FFF;
	newPulseIndex = 4096 - SIDpulseWidth;
#if defined(DIRECT_FIXPOINT)
	if (((waveStep.w.h + pulseIndex) >= 0x1000) && ((waveStep.w.h + newPulseIndex) >= 0x1000))
	{
		pulseIndex = newPulseIndex;
	}
	else if (((waveStep.w.h + pulseIndex) < 0x1000) && ((waveStep.w.h + newPulseIndex) < 0x1000))
	{
		pulseIndex = newPulseIndex;
	}
#else
	if (((waveStep + pulseIndex) >= 0x1000) && ((waveStep + newPulseIndex) >= 0x1000))
	{
		pulseIndex = newPulseIndex;
	}
	else if (((waveStep + pulseIndex) < 0x1000) && ((waveStep + newPulseIndex) < 0x1000))
	{
		pulseIndex = newPulseIndex;
	}
#endif

	uint8_t const oldWave = SIDctrl;
	uint8_t const newWave = reg[4] | (reg[5] << 8); // FIXME: what's actually supposed to happen here?
	uint8_t enveTemp = ADSRctrl;
	SIDctrl = newWave;

	if (!(newWave & 1))
	{
		if (oldWave & 1)
			enveTemp = ENVE_STARTRELEASE;
#if 0
		else if (gateOnCtrl)
			enveTemp = ENVE_STARTSHORTATTACK;
#endif
	}
	else if (/*gateOffCtrl || */!(oldWave & 1))
	{
		enveTemp = ENVE_STARTATTACK;
	}

	if ((oldWave ^ newWave) & 0xF0)
		cycleLenCount = 0;

	uint8_t const ADtemp = reg[5];
	uint8_t const SRtemp = reg[6];
	if (SIDAD != ADtemp)
		enveTemp |= ENVE_ALTER;
	else if (SIDSR != SRtemp)
		enveTemp |= ENVE_ALTER;

	SIDAD = ADtemp;
	SIDSR = SRtemp;
	uint8_t const tmpSusVol = masterVolumeLevels[SRtemp >> 4];
	if (ADSRctrl != ENVE_SUSTAIN)  // !!!
		enveSusVol = tmpSusVol;
	else if (enveSusVol > enveVol)
		enveSusVol = 0;
	else
		enveSusVol = tmpSusVol;

	ADSRproc = enveModeTable[enveTemp >> 1];  // shifting out the KEY-bit
	ADSRctrl = enveTemp & (255 - ENVE_ALTER - 1);

	filtEnabled = filter.Enabled && (reg[9] & 4);
}


/* -------------------------------------------------- Operator frame set-up 2 */

void jkm8580Operator::set2()
{
	outProc = &jkm8580Operator::wave_calc_normal;
	sync = false;

	if ((SIDfreq < 16) || (SIDctrl & 8))
	//if (/*(SIDfreq < 16) || */(SIDctrl & 8))
	{
		outProc = waveCalcMute;
		if (SIDfreq == 0)
		{
#if defined(DIRECT_FIXPOINT)
			cycleLen.l = cycleAddLen.l = 0;
			waveStep.l = 0;
#else
			cycleLen = cycleLenPnt = 0;
			cycleAddLenPnt = 0;
			waveStep = 0;
			waveStepPnt = 0;
#endif
			curSIDfreq = curNoiseFreq = 0;
			noiseStepAdd = 0;
			cycleLenCount = 0;
		}
		if (SIDctrl & 8)
		{
			if (noiseIsLocked)
			{
				noiseIsLocked = false;
#if defined(DIRECT_FIXPOINT)
				noiseReg.l = noiseSeed;
#else
				noiseReg = noiseSeed;
#endif
			}
		}
	}
	else
	{
		if (curSIDfreq != SIDfreq)
		{
			curSIDfreq = SIDfreq;
			// We keep the value cycleLen between 1 <= x <= 65535.
			// This makes a range-check in wave_calc_cycle_len() unrequired.
#if defined(DIRECT_FIXPOINT)
			cycleLen.l = ((sid->PCMsid << 12) / SIDfreq) << 4;
			if (cycleLenCount > 0)
			{
				wave_calc_cycle_len();
				outProc = &waveCalcRangeCheck;
			}
#else
			cycleLen = sid->PCMsid / SIDfreq;
			cycleLenPnt = ((sid->PCMsid % SIDfreq) * 65536UL) / SIDfreq;
			if (cycleLenCount > 0)
			{
				wave_calc_cycle_len();
				outProc = &waveCalcRangeCheck;
			}
#endif
		}

		if ((SIDctrl & 0x80) && (curNoiseFreq != SIDfreq))
		{
			curNoiseFreq = SIDfreq;
			noiseStepAdd = (sid->PCMsidNoise * SIDfreq) >> 8;
			if (noiseStepAdd >= (1L << 21))
			{
				sidModeNormalTable[0][8] = sidMode80hp;
				sidModeNormalTable[1][8] = sidMode80hp;
			}
			else
			{
				sidModeNormalTable[0][8] = sidMode80;
				sidModeNormalTable[1][8] = sidMode80;
			}
		}

		if (SIDctrl & 2)
		{
			if (!modulator->SIDfreq || (modulator->SIDctrl & 8))
			{
			}
			else if ((carrier->SIDctrl & 2) && (modulator->SIDfreq >= (SIDfreq << 1)))
			{
			}
			else
			{
				sync = true;
			}
		}

	if (((SIDctrl & 0x14) == 0x14) && modulator->SIDfreq)
		waveProc = sidModeRingTable[sidprocessmode][SIDctrl >> 4];
	else
		waveProc = sidModeNormalTable[sidprocessmode][SIDctrl >> 4];

	if ((SIDctrl & 0xc0) == 0xc0)
		outProc = &jkm8580Operator::wave_calc_digi;
	}
}


int8_t jkm8580Operator::wave_calc_normal(jkm8580Operator* pVoice)
{
	if (pVoice->cycleLenCount <= 0)
	{
		pVoice->wave_calc_cycle_len();
		if (pVoice->SIDctrl & 0x40)
		{
			pVoice->pulseIndex = pVoice->newPulseIndex;
			if (pVoice->pulseIndex > 2048)
			{
#if defined(DIRECT_FIXPOINT)
				pVoice->waveStep.w.h = 0;
#else
				pVoice->waveStep = 0;
#endif
			}
		}
	}

	(*pVoice->waveProc)(pVoice);
	pVoice->filtIO = ampMod1x8[(*pVoice->ADSRproc)(pVoice) | pVoice->output];
	//pVoice->filtIO = pVoice->masterVolume; // test for digi sound
	waveCalcFilter(pVoice);
	return pVoice->filtIO;
}

int8_t jkm8580Operator::wave_calc_digi(jkm8580Operator* pVoice)
{
	if (pVoice->cycleLenCount <= 0)
	{
		pVoice->wave_calc_cycle_len();
		if (pVoice->SIDctrl & 0x40)
		{
			pVoice->pulseIndex = pVoice->newPulseIndex;
			if (pVoice->pulseIndex > 2048)
			{
#if defined(DIRECT_FIXPOINT)
				pVoice->waveStep.w.h = 0;
#else
				pVoice->waveStep = 0;
#endif
			}
		}
	}

	(*pVoice->waveProc)(pVoice);
	pVoice->filtIO = s8(pVoice->output ^ 0x80);
	waveCalcFilter(pVoice);
	return pVoice->filtIO;
}


inline void jkm8580Operator::wave_calc_cycle_len()
{
#if defined(DIRECT_FIXPOINT)
	cycleAddLen.w.h = 0;
	cycleAddLen.l += cycleLen.l;
	cycleLenCount = cycleAddLen.w.h;
#else
	cycleAddLenPnt += cycleLenPnt;
	cycleLenCount = cycleLen;
	if (cycleAddLenPnt > 65535)
		cycleLenCount++;
	cycleAddLenPnt &= 0xFFFF;
#endif
	// If we keep the value cycleLen between 1 <= x <= 65535, the following check is not required.
#if 0
	if (!cycleLenCount)
	{
#if defined(DIRECT_FIXPOINT)
		waveStep.l = 0;
#else
		waveStep = waveStepPnt = 0;
#endif
		cycleLenCount = 0;
	}
	else
#endif
	{
#if defined(DIRECT_FIXPOINT)
		uint16_t diff = cycleLenCount - cycleLen.w.h;
#else
		uint16_t diff = cycleLenCount - cycleLen;
#endif
		if (wavePre[diff].len != cycleLenCount)
		{
			wavePre[diff].len = cycleLenCount;
#if defined(DIRECT_FIXPOINT)
			wavePre[diff].stp = waveStepAdd.l = (4096UL*65536UL) / cycleLenCount;
#else
			wavePre[diff].stp = waveStepAdd = 4096UL / cycleLenCount;
			wavePre[diff].pnt = waveStepAddPnt = ((4096UL % cycleLenCount) * 65536UL) / cycleLenCount;
#endif
		}
		else
		{
#if defined(DIRECT_FIXPOINT)
			waveStepAdd.l = wavePre[diff].stp;
#else
			waveStepAdd = wavePre[diff].stp;
			waveStepAddPnt = wavePre[diff].pnt;
#endif
		}
	}  // see above (opening bracket)
}


void sid2InitWaveformTables()
{
	int i,j;
	uint16_t k;

	k = 0;
	for (i = 0; i < 256; i++)
		for (j = 0; j < 8; j++)
			triangleTable[k++] = i;
	for (i = 255; i >= 0; i--)
		for (j = 0; j < 8; j++)
			triangleTable[k++] = i;

	k = 0;
	for (i = 0; i < 256; i++)
		for (j = 0; j < 16; j++)
			sawtoothTable[k++] = i;

	k = 0;
	for (i = 0; i < 4096; i++)
		squareTable[k++] = 255; //0; my estimation; especial for digi sound
	for (i = 0; i < 4096; i++)
		squareTable[k++] = 0; //255;

	waveform30[0] = waveform30_6581;
	waveform50[0] = waveform50_6581;
	waveform60[0] = waveform60_6581;
	waveform70[0] = waveform70_6581;
	waveform30[1] = waveform30_8580;
	waveform50[1] = waveform50_8580;
	waveform60[1] = waveform60_8580;
	waveform70[1] = waveform70_8580;

	sidModeNormalTable[0][3] = sidMode30;
	sidModeNormalTable[0][6] = sidMode60;
	sidModeNormalTable[0][7] = sidMode00;  /* really audible? */
	sidModeRingTable[0][7] = sidMode00;    /* */
	sidModeNormalTable[1][3] = sidMode30;
	sidModeNormalTable[1][6] = sidMode60;
	sidModeNormalTable[1][7] = sidMode70;
	sidModeRingTable[1][7] = sidMode74;

	{
#if defined(LARGE_NOISE_TABLE)
	uint32_t ni;
	for (ni = 0; ni < sizeof(noiseTableLSB); ni++)
	{
		noiseTableLSB[ni] = (uint8_t)
			(((ni >> (13-4)) & 0x10) |
				((ni >> (11-3)) & 0x08) |
				((ni >> (7-2)) & 0x04) |
				((ni >> (4-1)) & 0x02) |
				((ni >> (2-0)) & 0x01));
	}
	for (ni = 0; ni < sizeof(noiseTableMSB); ni++)
	{
		noiseTableMSB[ni] = (uint8_t)
			(((ni << (7-(22-16))) & 0x80) |
				((ni << (6-(20-16))) & 0x40) |
				((ni << (5-(16-16))) & 0x20));
	}
#else
	uint32_t ni;
	for (ni = 0; ni < sizeof(noiseTableLSB); ni++)
	{
		noiseTableLSB[ni] = (uint8_t)
			(((ni >> (7-2)) & 0x04) |
				((ni >> (4-1)) & 0x02) |
				((ni >> (2-0)) & 0x01));
	}
	for (ni = 0; ni < sizeof(noiseTableMID); ni++)
	{
		noiseTableMID[ni] = (uint8_t)
			(((ni >> (13-8-4)) & 0x10) |
				((ni << (3-(11-8))) & 0x08));
	}
	for (ni = 0; ni < sizeof(noiseTableMSB); ni++)
	{
		noiseTableMSB[ni] = (uint8_t)
			(((ni << (7-(22-16))) & 0x80) |
				((ni << (6-(20-16))) & 0x40) |
				((ni << (5-(16-16))) & 0x20));
	}
#endif
	}
}

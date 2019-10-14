// license:BSD-3-Clause
// copyright-holders:Peter Trauner
#include "emu.h"
#include "sid4voice.h"

#include "sid4.h"
#include "sid4envel.h"

#include "sound/jkm6581.h"


#if defined(LARGE_NOISE_TABLE)
	static u8 noiseTableMSB[1<<8];
	static u8 noiseTableLSB[1L<<16];
#else
	static u8 noiseTableMSB[1<<8];
	static u8 noiseTableMID[1<<8];
	static u8 noiseTableLSB[1<<8];
#endif


static std::unique_ptr<s8[]> ampMod1x8;

static const u32 noiseSeed = 0x7ffff8;

void sidInitMixerEngine(running_machine &machine)
{
	u16 uk;
	s32 si, sj;

	/* 8-bit volume modulation tables. */
	float filterAmpl = 0.7f;

	ampMod1x8=std::make_unique<s8[]>(256*256);

	uk = 0;
	for ( si = 0; si < 256; si++ )
	{
		for ( sj = -128; sj < 128; sj++, uk++ )
		{
			ampMod1x8[uk] = (s8)(((si*sj)/255)*filterAmpl);
		}
	}

}

static inline void waveAdvance(sid4Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	pVoice->waveStep.l += pVoice->waveStepAdd.l;
	pVoice->waveStep.w[HI] &= pVoice->waveSize - 1;
#else
	pVoice->waveStepPnt += pVoice->waveStepAddPnt;
	pVoice->waveStep += pVoice->waveStepAdd;
	if (pVoice->waveStepPnt > 65535 ) pVoice->waveStep++;
	pVoice->waveStepPnt &= 0xFFFF;
	pVoice->waveStep &= (pVoice->waveSize << 2) - 1;
#endif
	pVoice->reg[14] = pVoice->output;
	pVoice->reg[15] = pVoice->enveVol;
}

static inline void noiseAdvance(sid4Operator* pVoice)
{
	pVoice->noiseStep += pVoice->noiseStepAdd;
	if (pVoice->noiseStep >= (1L<<20))
	{
		pVoice->noiseStep -= (1L<<20);
#if defined(DIRECT_FIXPOINT)
		pVoice->noiseReg.l = (pVoice->noiseReg.l << 1) |
			(((pVoice->noiseReg.l >> 22) ^ (pVoice->noiseReg.l >> 17)) & 1);
#else
		pVoice->noiseReg = (pVoice->noiseReg << 1) |
			(((pVoice->noiseReg >> 22) ^ (pVoice->noiseReg >> 17)) & 1);
#endif
#if defined(DIRECT_FIXPOINT) && defined(LARGE_NOISE_TABLE)
		pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.w[LO]]
								|noiseTableMSB[pVoice->noiseReg.w[HI]&0xff]);
#elif defined(DIRECT_FIXPOINT)
		pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.b[LOLO]]
								|noiseTableMID[pVoice->noiseReg.b[LOHI]]
								|noiseTableMSB[pVoice->noiseReg.b[HILO]]);
#else
		pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg&0xff]
								|noiseTableMID[pVoice->noiseReg>>8&0xff]
								|noiseTableMSB[pVoice->noiseReg>>16&0xff]);
#endif
	}
}

static inline void noiseAdvanceHp(sid4Operator* pVoice)
{
	u32 tmp = pVoice->noiseStepAdd;
	while (tmp >= (1L<<20))
	{
		tmp -= (1L<<20);
#if defined(DIRECT_FIXPOINT)
		pVoice->noiseReg.l = (pVoice->noiseReg.l << 1) |
			(((pVoice->noiseReg.l >> 22) ^ (pVoice->noiseReg.l >> 17)) & 1);
#else
		pVoice->noiseReg = (pVoice->noiseReg << 1) |
			(((pVoice->noiseReg >> 22) ^ (pVoice->noiseReg >> 17)) & 1);
#endif
	}
	pVoice->noiseStep += tmp;
	if (pVoice->noiseStep >= (1L<<20))
	{
		pVoice->noiseStep -= (1L<<20);
#if defined(DIRECT_FIXPOINT)
		pVoice->noiseReg.l = (pVoice->noiseReg.l << 1) |
			(((pVoice->noiseReg.l >> 22) ^ (pVoice->noiseReg.l >> 17)) & 1);
#else
		pVoice->noiseReg = (pVoice->noiseReg << 1) |
			(((pVoice->noiseReg >> 22) ^ (pVoice->noiseReg >> 17)) & 1);
#endif
	}
#if defined(DIRECT_FIXPOINT) && defined(LARGE_NOISE_TABLE)
	pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.w[LO]]
							|noiseTableMSB[pVoice->noiseReg.w[HI]&0xff]);
#elif defined(DIRECT_FIXPOINT)
	pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg.b[LOLO]]
							|noiseTableMID[pVoice->noiseReg.b[LOHI]]
							|noiseTableMSB[pVoice->noiseReg.b[HILO]]);
#else
	pVoice->noiseOutput = (noiseTableLSB[pVoice->noiseReg&0xff]
							|noiseTableMID[pVoice->noiseReg>>8&0xff]
							|noiseTableMSB[pVoice->noiseReg>>16&0xff]);
#endif
}


static void sidMode00(sid4Operator* pVoice)  {
	pVoice->output = (pVoice->filtIO-0x80);
	waveAdvance(pVoice);
}

#if 0
/* not used */
static void sidModeReal00(sid4Operator* pVoice)  {
	pVoice->output = 0;
	waveAdvance(pVoice);
}
#endif

static void sidMode10(sid4Operator* pVoice)  {
#if defined(DIRECT_FIXPOINT)
	pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
#else
	pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
#endif
	waveAdvance(pVoice);
}

static void sidMode30(sid4Operator* pVoice)  {
#if defined(DIRECT_FIXPOINT)
	if ( pVoice->waveStep.w[HI] < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep.w[HI] & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));
#else
	if ( pVoice->waveStep < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));
#endif
	waveAdvance(pVoice);
}

static void sidMode50(sid4Operator* pVoice)  {
#if defined(DIRECT_FIXPOINT)
	if ( pVoice->waveStep.w[HI] < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = 0xff ^ pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
#else
	if ( pVoice->waveStep < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = 0xff ^ pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
#endif
	waveAdvance(pVoice);
}

static void sidMode70(sid4Operator* pVoice)  {
#if defined(DIRECT_FIXPOINT)
	if ( pVoice->waveStep.w[HI] < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep.w[HI] & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->waveStep.w[HI] & (pVoice->waveSize << 1) )
		pVoice->output ^= 0xff;
#else
	if ( pVoice->waveStep < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->waveStep & (pVoice->waveSize << 1) )
		pVoice->output ^= 0xff;
#endif
	waveAdvance(pVoice);
}

static void sidMode80(sid4Operator* pVoice)  {
	pVoice->output = pVoice->noiseOutput;
	waveAdvance(pVoice);
	noiseAdvance(pVoice);
}

static void sidMode80hp(sid4Operator* pVoice)  {
	pVoice->output = pVoice->noiseOutput;
	waveAdvance(pVoice);
	noiseAdvanceHp(pVoice);
}

static void sidModeLock(sid4Operator* pVoice)
{
	pVoice->noiseIsLocked = true;
	pVoice->output = (pVoice->filtIO-0x80);
	waveAdvance(pVoice);
}

/* */
/* */
/* */

static void sidMode14(sid4Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->modulator->waveStep.w[HI] & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#else
	pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->modulator->waveStep & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#endif
	waveAdvance(pVoice);
}

static void sidMode34(sid4Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if ( pVoice->waveStep.w[HI] & pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep.w[HI] & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->modulator->waveStep.w[HI] & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#else
	if ( pVoice->waveStep & pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->modulator->waveStep & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#endif
	waveAdvance(pVoice);
}

static void sidMode54(sid4Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if ( pVoice->waveStep.w[HI] < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = 0xff ^ pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->modulator->waveStep.w[HI] & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#else
	if ( pVoice->waveStep < pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = 0xff ^ pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->modulator->waveStep & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#endif
	waveAdvance(pVoice);
}

static void sidMode74(sid4Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	if ( pVoice->waveStep.w[HI] & pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep.w[HI] & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep.w[HI] & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->waveStep.w[HI] & (pVoice->waveSize << 1) )
		pVoice->output ^= 0xff;

	if ( pVoice->modulator->waveStep.w[HI] & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#else
	if ( pVoice->waveStep & pVoice->waveSize )
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + (pVoice->waveStep & (pVoice->waveSize - 1))) & ((1 << 16) - 1));
	else
		pVoice->output = pVoice->sid->cache->read_byte((pVoice->SIDpulseWidth + ((pVoice->waveStep & (pVoice->waveSize - 1)) ^ (pVoice->waveSize - 1))) & ((1 << 16) - 1));

	if ( pVoice->waveStep & (pVoice->waveSize << 1) )
		pVoice->output ^= 0xff;

	if ( pVoice->modulator->waveStep & (pVoice->modulator->waveSize >> 1) )
		pVoice->output ^= 0xff;
#endif
	waveAdvance(pVoice);
}

/* */
/* */
/* */

static inline void waveCalcFilter(sid4Operator* pVoice)
{
	if ( pVoice->filtEnabled )
	{
		if ( pVoice->filter.Type != 0 )
		{
			if ( pVoice->filter.Type == 0x20 )
			{
				float tmp;
				pVoice->filtLow += ( pVoice->filtRef * pVoice->filter.Dy );
				tmp = (float)pVoice->filtIO - pVoice->filtLow;
				tmp -= pVoice->filtRef * pVoice->filter.ResDy;
				pVoice->filtRef += ( tmp * (pVoice->filter.Dy) );
				pVoice->filtIO = (s8)(pVoice->filtRef-pVoice->filtLow/4);
			}
			else if (pVoice->filter.Type == 0x40)
			{
				float tmp, tmp2;
				pVoice->filtLow += ( pVoice->filtRef * pVoice->filter.Dy * 0.1f );
				tmp = (float)pVoice->filtIO - pVoice->filtLow;
				tmp -= pVoice->filtRef * pVoice->filter.ResDy;
				pVoice->filtRef += ( tmp * (pVoice->filter.Dy) );
				tmp2 = pVoice->filtRef - pVoice->filtIO/8;
				if (tmp2 < -128)
					tmp2 = -128;
				if (tmp2 > 127)
					tmp2 = 127;
				pVoice->filtIO = (s8)tmp2;
			}
			else
			{
				float sample, sample2;
				int tmp;
				pVoice->filtLow += ( pVoice->filtRef * pVoice->filter.Dy );
				sample = pVoice->filtIO;
				sample2 = sample - pVoice->filtLow;
				tmp = (int)sample2;
				sample2 -= pVoice->filtRef * pVoice->filter.ResDy;
				pVoice->filtRef += ( sample2 * pVoice->filter.Dy );

				if ( pVoice->filter.Type == 0x10 )
				{
					pVoice->filtIO = (s8)pVoice->filtLow;
				}
				else if ( pVoice->filter.Type == 0x30 )
				{
					pVoice->filtIO = (s8)pVoice->filtLow;
				}
				else if ( pVoice->filter.Type == 0x50 )
				{
					pVoice->filtIO = (s8)(sample - (tmp >> 1));
				}
				else if ( pVoice->filter.Type == 0x60 )
				{
					pVoice->filtIO = (s8)tmp;
				}
				else if ( pVoice->filter.Type == 0x70 )
				{
					pVoice->filtIO = (s8)(sample - (tmp >> 1));
				}
			}
		}
		else /* pVoice->filter.Type == 0x00 */
		{
			pVoice->filtIO = 0;
		}
	}
}

static s8 waveCalcMute(sid4Operator* pVoice)
{
	(*pVoice->ADSRproc)(pVoice);  /* just process envelope */
	return pVoice->filtIO;//&pVoice->outputMask;
}


static s8 waveCalcRangeCheck(sid4Operator* pVoice)
{
#if defined(DIRECT_FIXPOINT)
	pVoice->waveStepOld = pVoice->waveStep.w[HI];
	(*pVoice->waveProc)(pVoice);
	if (pVoice->waveStep.w[HI] < pVoice->waveStepOld)
#else
	pVoice->waveStepOld = pVoice->waveStep;
	(*pVoice->waveProc)(pVoice);
	if (pVoice->waveStep < pVoice->waveStepOld)
#endif
	{
		/* Next step switch back to normal calculation. */
		pVoice->cycleLenCount = 0;
		pVoice->outProc = &sid4Operator::wave_calc_normal;
#if defined(DIRECT_FIXPOINT)
				pVoice->waveStep.w[HI] = (pVoice->waveSize << 2) - 1;
#else
				pVoice->waveStep = (pVoice->waveSize << 2) - 1;
#endif
	}
	pVoice->filtIO = ampMod1x8[(*pVoice->ADSRproc)(pVoice)|pVoice->output];
	waveCalcFilter(pVoice);
	return pVoice->filtIO;//&pVoice->outputMask;
}

/* MOS-8580, MOS-6581 (no 70) */
static ptr2sidVoidFunc sidModeNormalTable[16] =
{
	sidMode00, sidMode10, sidMode00, sidMode30, sidMode00, sidMode50, sidMode00, sidMode70,
	sidMode80, sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock
};

/* MOS-8580, MOS-6581 (no 74) */
static ptr2sidVoidFunc sidModeRingTable[16] =
{
	sidMode00, sidMode14, sidMode00, sidMode34, sidMode00, sidMode54, sidMode00, sidMode74,
	sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock, sidModeLock
};


void sid4Operator::clear()
{
	SIDfreq = 0;
	SIDctrl = 0;
	SIDAD = 0;
	SIDSR = 0;

	sync = false;

	pulseIndex = newPulseIndex = SIDpulseWidth = 0;
	curSIDfreq = curNoiseFreq = 0;

	output = noiseOutput = 0;
	outputLMask = outputRMask = ~0;
	filtIO = 0;

	filtEnabled = false;
	filtLow = filtRef = 0;

	filter.Type = filter.CurType = 0;
	filter.Value = 0;
	filter.Dy = filter.ResDy = 0;

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

void sid4Operator::filterTableInit(running_machine &machine)
{
	u32 uk;
	/* Parameter calculation has not been moved to a separate function */
	/* by purpose. */
	float resDyMax;
	float resDyMin;
	float resDy;

	filterTable = std::make_unique<float[]>(0x10000);

	for ( uk = 0; uk < 0x10000; uk++ )
	{
		filterTable[uk] = (1.0f + float(uk)) / 65536.0f;
	}

	/*extern float filterResTable[16]; */
	resDyMax = 1.0f;
	resDyMin = 2.0f;
	resDy = resDyMin;
	for ( uk = 0; uk < 16; uk++ )
	{
		filterResTable[uk] = resDy;
		resDy -= (( resDyMin - resDyMax ) / 15 );
	}
	filterResTable[0] = resDyMin;
	filterResTable[15] = resDyMax;
}


/* -------------------------------------------------- Operator frame set-up 1 */

void sid4Operator::set()
{
	SIDfreq = reg[0] | (reg[1] << 8);

	SIDpulseWidth = (reg[2] | (reg[3] << 8)) & 0xffff;

	u8 const oldWave = SIDctrl;
	u8 const newWave = reg[4] | (reg[5] << 8); // FIXME: what's actually supposed to happen here?
	u8 enveTemp = ADSRctrl;
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

	// filter mode (reg[12] & 0x8) >> 3;
	// filter bias (reg[7] & 0xf0) >> 4;
	waveSize = 1 << (reg[7] & 0xF);

	u8 const ADtemp = reg[5];
	u8 const SRtemp = reg[6];
	if (SIDAD != ADtemp)
		enveTemp |= ENVE_ALTER;
	else if (SIDSR != SRtemp)
		enveTemp |= ENVE_ALTER;

	SIDAD = ADtemp;
	SIDSR = SRtemp;
	u8 const tmpSusVol = masterVolumeLevels[SRtemp >> 4];
	if (ADSRctrl != ENVE_SUSTAIN)  // !!!
		enveSusVol = tmpSusVol;
	else if (enveSusVol > enveVol)
		enveSusVol = 0;
	else
		enveSusVol = tmpSusVol;

	ADSRproc = enveModeTable[enveTemp >> 1];  // shifting out the KEY-bit
	ADSRctrl = enveTemp & (255 - ENVE_ALTER - 1);

	masterVolume = reg[13] & 15;
	masterVolumeAmplIndex = masterVolume << 8;

	if ((reg[13] & 0x80) && !(reg[7] & 2))
		outputLMask = 0;     /* off */
	else
		outputLMask = ~0;  /* on */

	if ((reg[13] & 0x80) && !(reg[7] & 4))
		outputRMask = 0;     /* off */
	else
		outputRMask = ~0;  /* on */

	filtEnabled = filter.Enabled && (reg[12] & 1);

	filter.Type = reg[13] & 0x70;
	if (filter.Type != filter.CurType)
	{
		filter.CurType = filter.Type;
		filtLow = filtRef = 0;
	}
	if (filter.Enabled)
	{
		filter.Value = ((reg[10] & 0xff) | (u16(reg[11]) << 8));
		filter.Dy = filterTable ? filterTable[filter.Value] : 0.0f;
		filter.ResDy = filterResTable[reg[12] >> 4] - filter.Dy;
		if (filter.ResDy < 1.0f)
			filter.ResDy = 1.0f;
	}

	lvol = reg[8];
	rvol = reg[9];
}


/* -------------------------------------------------- Operator frame set-up 2 */

void sid4Operator::set2()
{
	outProc = &sid4Operator::wave_calc_normal;
	sync = false;

	if ((SIDfreq < 1) || (SIDctrl & 8))
	//if (/*(SIDfreq < 1) || */(SIDctrl & 8))
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
				sidModeNormalTable[8] = sidMode80hp;
			else
				sidModeNormalTable[8] = sidMode80;
		}

		if (SIDctrl & 2)
		{
			if (!modulator->SIDfreq || (modulator->SIDctrl & 8 ))
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

	if (((SIDctrl & 0x14 ) == 0x14) && modulator->SIDfreq)
		waveProc = sidModeRingTable[SIDctrl >> 4];
	else
		waveProc = sidModeNormalTable[SIDctrl >> 4];
	}
}


s8 sid4Operator::wave_calc_normal(sid4Operator* pVoice)
{
	if (pVoice->cycleLenCount <= 0)
	{
		pVoice->wave_calc_cycle_len();
	}

	(*pVoice->waveProc)(pVoice);
	pVoice->filtIO = ampMod1x8[(*pVoice->ADSRproc)(pVoice) | pVoice->output];
	//pVoice->filtIO = pVoice->sid->masterVolume; // test for digi sound
	waveCalcFilter(pVoice);
	return pVoice->filtIO;//&pVoice->outputMask;
}


inline void sid4Operator::wave_calc_cycle_len()
{
#if defined(DIRECT_FIXPOINT)
	cycleAddLen.w[HI] = 0;
	cycleAddLen.l += cycleLen.l;
	cycleLenCount = cycleAddLen.w[HI];
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
		u16 diff = cycleLenCount - cycleLen.w[HI];
#else
		u16 diff = cycleLenCount - cycleLen;
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


void sidInitWaveformTables()
{
	int i,j;
	u16 k;

	{
#if defined(LARGE_NOISE_TABLE)
	u32 ni;
	for (ni = 0; ni < sizeof(noiseTableLSB); ni++)
	{
		noiseTableLSB[ni] = (u8)
			(((ni >> (13-4)) & 0x10) |
				((ni >> (11-3)) & 0x08) |
				((ni >> (7-2)) & 0x04) |
				((ni >> (4-1)) & 0x02) |
				((ni >> (2-0)) & 0x01));
	}
	for (ni = 0; ni < sizeof(noiseTableMSB); ni++)
	{
		noiseTableMSB[ni] = (u8)
			(((ni << (7-(22-16))) & 0x80) |
				((ni << (6-(20-16))) & 0x40) |
				((ni << (5-(16-16))) & 0x20));
	}
#else
	u32 ni;
	for (ni = 0; ni < sizeof(noiseTableLSB); ni++)
	{
		noiseTableLSB[ni] = (u8)
			(((ni >> (7-2)) & 0x04) |
				((ni >> (4-1)) & 0x02) |
				((ni >> (2-0)) & 0x01));
	}
	for (ni = 0; ni < sizeof(noiseTableMID); ni++)
	{
		noiseTableMID[ni] = (u8)
			(((ni >> (13-8-4)) & 0x10) |
				((ni << (3-(11-8))) & 0x08));
	}
	for (ni = 0; ni < sizeof(noiseTableMSB); ni++)
	{
		noiseTableMSB[ni] = (u8)
			(((ni << (7-(22-16))) & 0x80) |
				((ni << (6-(20-16))) & 0x40) |
				((ni << (5-(16-16))) & 0x20));
	}
#endif
	}
}

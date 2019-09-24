// license:BSD-3-Clause
// copyright-holders:Peter Trauner
#ifndef MAME_SOUND_SID4VOICE_H
#define MAME_SOUND_SID4VOICE_H

#pragma once


#define lowPassParam filterTable
/*
  approximation of the jkm6581 chip
  this part is for 1 (of the 3) voices of a chip
*/
struct JKM6581_t;

struct sid4Operator
{
	struct sw_storage
	{
		u16 len;
#if defined(DIRECT_FIXPOINT)
		u32 stp;
#else
		u32 pnt;
		s16 stp;
#endif
	};

	struct
	{
		int Enabled;
		u8 Type, CurType;
		float Dy, ResDy;
		u16 Value;
	} filter;

	u8 masterVolume;
	u16 masterVolumeAmplIndex;

	u8 lvol, rvol;
	JKM6581_t *sid;
	u8 reg[0x10];
	u32 SIDfreq;
	u16 SIDpulseWidth;
	u8 SIDctrl;
	u8 SIDAD, SIDSR;

	sid4Operator* carrier;
	sid4Operator* modulator;
	int sync;

	u16 pulseIndex, newPulseIndex;
	u16 curSIDfreq;
	u16 curNoiseFreq;

	u8 output, outputMask;

	int filtEnabled;
	float filtLow, filtRef;
	s8 filtIO;

	s32 cycleLenCount;
#if defined(DIRECT_FIXPOINT)
	cpuLword cycleLen, cycleAddLen;
#else
	u32 cycleAddLenPnt;
	u16 cycleLen, cycleLenPnt;
#endif

	s8 (*outProc)(sid4Operator *);
	void (*waveProc)(sid4Operator *);

#if defined(DIRECT_FIXPOINT)
	cpuLword waveStep, waveStepAdd;
#else
	u16 waveStep, waveStepAdd;
	u32 waveStepPnt, waveStepAddPnt;
#endif
	u16 waveStepOld;
	u16 waveSize;
	sw_storage wavePre[2];

#if defined(DIRECT_FIXPOINT) && defined(LARGE_NOISE_TABLE)
	cpuLword noiseReg;
#elif defined(DIRECT_FIXPOINT)
	cpuLBword noiseReg;
#else
	u32 noiseReg;
#endif
	u32 noiseStep, noiseStepAdd;
	u8 noiseOutput;
	int noiseIsLocked;

	u8 ADSRctrl;
//  int gateOnCtrl, gateOffCtrl;
	u16 (*ADSRproc)(sid4Operator *);

#ifdef SID_FPUENVE
	float fenveStep, fenveStepAdd;
	u32 enveStep;
#elif defined(DIRECT_FIXPOINT)
	cpuLword enveStep, enveStepAdd;
#else
	u16 enveStep, enveStepAdd;
	u32 enveStepPnt, enveStepAddPnt;
#endif
	u8 enveVol, enveSusVol;
	u16 enveShortAttackCount;

	std::unique_ptr<float[]> filterTable;
	std::unique_ptr<float[]> bandPassParam;
	float filterResTable[16];

	void clear();

	void filterTableInit(running_machine &machine);
	void set();
	void set2();
	static s8 wave_calc_normal(sid4Operator *pVoice);

private:
	void wave_calc_cycle_len();
};

typedef s8 (*ptr2sidFunc)(sid4Operator *);
typedef u16 (*ptr2sidUwordFunc)(sid4Operator *);
typedef void (*ptr2sidVoidFunc)(sid4Operator *);

void sidInitWaveformTables();
void sidInitMixerEngine(running_machine &machine);

#if 0
extern ptr2sidVoidFunc sid6581ModeNormalTable[16];
extern ptr2sidVoidFunc sid6581ModeRingTable[16];
extern ptr2sidVoidFunc sid8580ModeNormalTable[16];
extern ptr2sidVoidFunc sid8580ModeRingTable[16];
#endif

#endif // MAME_SOUND_SID4VOICE_H

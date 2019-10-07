// license:BSD-3-Clause
// copyright-holders:Peter Trauner
#ifndef MAME_SOUND_SID2VOICE_H
#define MAME_SOUND_SID2VOICE_H

#pragma once


/*
  approximation of the sid6581 chip
  this part is for 1 (of the 3) voices of a chip
*/
struct JKM8580_t;

struct jkm8580Operator
{
	struct sw_storage
	{
		uint16_t len;
#if defined(DIRECT_FIXPOINT)
		uint32_t stp;
#else
		uint32_t pnt;
		int16_t stp;
#endif
	};

	JKM8580_t *sid;
	int sidwavemode;
	int sidprocessmode;
	uint8_t reg[16];
	uint32_t SIDfreq;
	uint16_t SIDpulseWidth;
	uint8_t SIDctrl;
	uint8_t SIDAD, SIDSR;

	jkm8580Operator* carrier;
	jkm8580Operator* modulator;
	int sync;

	uint16_t pulseIndex, newPulseIndex;
	uint16_t curSIDfreq;
	uint16_t curNoiseFreq;

	uint8_t output, outputLMask, outputRMask;

	uint8_t masterVolume;
	uint16_t masterVolumeAmplIndex;

	struct
	{
		int Enabled;
		uint8_t Type, CurType;
		float Dy, ResDy;
		uint16_t Value;
	} filter;

	int filtEnabled;
	float filtLow, filtRef;
	int8_t filtIO;

	int32_t cycleLenCount;
#if defined(DIRECT_FIXPOINT)
	PAIR cycleLen, cycleAddLen;
#else
	uint32_t cycleAddLenPnt;
	uint16_t cycleLen, cycleLenPnt;
#endif

	int8_t (*outProc)(jkm8580Operator *);
	void (*waveProc)(jkm8580Operator *);

#if defined(DIRECT_FIXPOINT)
	PAIR waveStep, waveStepAdd;
#else
	uint16_t waveStep, waveStepAdd;
	uint32_t waveStepPnt, waveStepAddPnt;
#endif
	uint16_t waveStepOld;
	sw_storage wavePre[2];

#if defined(DIRECT_FIXPOINT)
	PAIR noiseReg;
#else
	uint32_t noiseReg;
#endif
	uint32_t noiseStep, noiseStepAdd;
	uint8_t noiseOutput;
	int noiseIsLocked;

	uint8_t ADSRctrl;
//  int gateOnCtrl, gateOffCtrl;
	uint16_t (*ADSRproc)(jkm8580Operator *);

#ifdef SID_FPUENVE
	float fenveStep, fenveStepAdd;
	uint32_t enveStep;
#elif defined(DIRECT_FIXPOINT)
	PAIR enveStep, enveStepAdd;
#else
	uint16_t enveStep, enveStepAdd;
	uint32_t enveStepPnt, enveStepAddPnt;
#endif
	uint8_t enveVol, enveSusVol;
	uint16_t enveShortAttackCount;

	void clear();

	void set();
	void set2();
	static int8_t wave_calc_normal(jkm8580Operator *pVoice);

private:
	void wave_calc_cycle_len();
};

typedef int8_t (*ptr2sidFunc)(jkm8580Operator *);
typedef uint16_t (*ptr2sidUwordFunc)(jkm8580Operator *);
typedef void (*ptr2sidVoidFunc)(jkm8580Operator *);

void sid2InitWaveformTables();
void sidInitMixerEngine(running_machine &machine);
void filterTableInit(running_machine &machine);

#if 0
extern ptr2sidVoidFunc sid6581ModeNormalTable[16];
extern ptr2sidVoidFunc sid6581ModeRingTable[16];
extern ptr2sidVoidFunc sid8580ModeNormalTable[16];
extern ptr2sidVoidFunc sid8580ModeRingTable[16];
#endif

#endif // MAME_SOUND_SID2VOICE_H

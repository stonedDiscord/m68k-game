#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "pt3player.h"

/*
 * Refactored PT3 player for m68k bare-metal.
 * Fixes for m68000:
 * 1. Removed packing from internal state structs to ensure 16-bit alignment.
 * 2. Explicit Little-Endian reading for PT3 data (unaligned safe).
 * 3. Removed Module struct to avoid alignment issues with the music header.
 */

/* PT3 File Header Offsets */
#define HDR_MusicName           0
#define HDR_TonTableId          99
#define HDR_Delay               100
#define HDR_NumberOfPositions   101
#define HDR_LoopPosition        102
#define HDR_PatternsPointer     103
#define HDR_SamplesPointers     105
#define HDR_OrnamentsPointers   169
#define HDR_PositionList        201

static inline uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

const uint16_t PT3NoteTable_PT_33_34r[96] = {
	0xC21,0xB73,0xACE,0xA33,0x9A0,0x916,0x893,0x818,0x7A4,0x736,0x6CE,0x66D,
	0x610,0x5B9,0x567,0x519,0x4D0,0x48B,0x449,0x40C,0x3D2,0x39B,0x367,0x336,
	0x308,0x2DC,0x2B3,0x28C,0x268,0x245,0x224,0x206,0x1E9,0x1CD,0x1B3,0x19B,
	0x184,0x16E,0x159,0x146,0x134,0x122,0x112,0x103,0x0F4,0x0E6,0x0D9,0x0CD,
	0x0C2,0x0B7,0x0AC,0x0A3,0x09A,0x091,0x089,0x081,0x07A,0x073,0x06C,0x066,
	0x061,0x05B,0x056,0x051,0x04D,0x048,0x044,0x040,0x03D,0x039,0x036,0x033,
	0x030,0x02D,0x02B,0x028,0x026,0x024,0x022,0x020,0x01E,0x01C,0x01B,0x019,
	0x018,0x016,0x015,0x014,0x013,0x012,0x011,0x010,0x00F,0x00E,0x00D,0x00C};

const uint16_t PT3NoteTable_PT_34_35[96] = {
	0xC22,0xB73,0xACF,0xA33,0x9A1,0x917,0x894,0x819,0x7A4,0x737,0x6CF,0x66D,
	0x611,0x5BA,0x567,0x51A,0x4D0,0x48B,0x44A,0x40C,0x3D2,0x39B,0x367,0x337,
	0x308,0x2DD,0x2B4,0x28D,0x268,0x246,0x225,0x206,0x1E9,0x1CE,0x1B4,0x19B,
	0x184,0x16E,0x15A,0x146,0x134,0x123,0x112,0x103,0x0F5,0x0E7,0x0DA,0x0CE,
	0x0C2,0x0B7,0x0AD,0x0A3,0x09A,0x091,0x089,0x082,0x07A,0x073,0x06D,0x067,
	0x061,0x05C,0x056,0x052,0x04D,0x049,0x045,0x041,0x03D,0x03A,0x036,0x033,
	0x031,0x02E,0x02B,0x029,0x027,0x024,0x022,0x020,0x01F,0x01D,0x01B,0x01A,
	0x018,0x017,0x016,0x014,0x013,0x012,0x011,0x010,0x00F,0x00E,0x00D,0x00C};

const uint16_t PT3NoteTable_ST[96] = {
	0xEF8,0xE10,0xD60,0xC80,0xBD8,0xB28,0xA88,0x9F0,0x960,0x8E0,0x858,0x7E0,
	0x77C,0x708,0x6B0,0x640,0x5EC,0x594,0x544,0x4F8,0x4B0,0x470,0x42C,0x3FD,
	0x3BE,0x384,0x358,0x320,0x2F6,0x2CA,0x2A2,0x27C,0x258,0x238,0x216,0x1F8,
	0x1DF,0x1C2,0x1AC,0x190,0x17B,0x165,0x151,0x13E,0x12C,0x11C,0x10A,0x0FC,
	0x0EF,0x0E1,0x0D6,0x0C8,0x0BD,0x0B2,0x0A8,0x09F,0x096,0x08E,0x085,0x07E,
	0x077,0x070,0x06B,0x064,0x05E,0x059,0x054,0x04F,0x04B,0x047,0x042,0x03F,
	0x03B,0x038,0x035,0x032,0x02F,0x02C,0x02A,0x027,0x025,0x023,0x021,0x01F,
	0x01D,0x01C,0x01A,0x019,0x017,0x016,0x015,0x013,0x012,0x011,0x010,0x00F};

const uint16_t PT3NoteTable_ASM_34r[96] = {
	0xD3E,0xC80,0xBCC,0xB22,0xA82,0x9EC,0x95C,0x8D6,0x858,0x7E0,0x76E,0x704,
	0x69F,0x640,0x5E6,0x591,0x541,0x4F6,0x4AE,0x46B,0x42C,0x3F0,0x3B7,0x382,
	0x34F,0x320,0x2F3,0x2C8,0x2A1,0x27B,0x257,0x236,0x216,0x1F8,0x1DC,0x1C1,
	0x1A8,0x190,0x179,0x164,0x150,0x13D,0x12C,0x11B,0x10B,0x0FC,0x0EE,0x0E0,
	0x0D4,0x0C8,0x0BD,0x0B2,0x0A8,0x09F,0x096,0x08D,0x085,0x07E,0x077,0x070,
	0x06A,0x064,0x05E,0x059,0x054,0x050,0x04B,0x047,0x043,0x03F,0x03C,0x038,
	0x035,0x032,0x02F,0x02D,0x02A,0x028,0x026,0x024,0x022,0x020,0x01E,0x01D,
	0x01B,0x01A,0x019,0x018,0x015,0x014,0x013,0x012,0x011,0x010,0x00F,0x00E};

const uint16_t PT3NoteTable_ASM_34_35[96] = {
	0xD10,0xC55,0xBA4,0xAFC,0xA5F,0x9CA,0x93D,0x8B8,0x83B,0x7C5,0x755,0x6EC,
	0x688,0x62A,0x5D2,0x57E,0x52F,0x4E5,0x49E,0x45C,0x41D,0x3E2,0x3AB,0x376,
	0x344,0x315,0x2E9,0x2BF,0x298,0x272,0x24F,0x22E,0x20F,0x1F1,0x1D5,0x1BB,
	0x1A2,0x18B,0x174,0x160,0x14C,0x139,0x128,0x117,0x107,0x0F9,0x0EB,0x0DD,
	0x0D1,0x0C5,0x0BA,0x0B0,0x0A6,0x09D,0x094,0x08C,0x084,0x07C,0x075,0x06F,
	0x069,0x063,0x05D,0x058,0x053,0x04E,0x04A,0x046,0x042,0x03E,0x03B,0x037,
	0x034,0x031,0x02F,0x02C,0x029,0x027,0x024,0x022,0x020,0x01F,0x01D,0x01B,
	0x01A,0x018,0x017,0x016,0x014,0x013,0x012,0x011,0x010,0x00F,0x00E,0x00D};

const uint16_t PT3NoteTable_REAL_34r[96] = {
	0xCDA,0xC22,0xB73,0xACF,0xA33,0x9A1,0x917,0x894,0x819,0x7A4,0x737,0x6CF,
	0x66D,0x611,0x5BA,0x567,0x51A,0x4D0,0x48B,0x44A,0x40C,0x3D2,0x39B,0x367,
	0x337,0x308,0x2DD,0x2B4,0x28D,0x268,0x246,0x225,0x206,0x1E9,0x1CE,0x1B4,
	0x19B,0x184,0x16E,0x15A,0x146,0x134,0x123,0x113,0x103,0x0F5,0x0E7,0x0DA,
	0x0CE,0x0C2,0x0B7,0x0AD,0x0A3,0x09A,0x091,0x089,0x082,0x07A,0x073,0x06D,
	0x067,0x061,0x05C,0x056,0x052,0x04D,0x049,0x045,0x041,0x03D,0x03A,0x036,
	0x033,0x031,0x02E,0x02B,0x029,0x027,0x024,0x022,0x020,0x01F,0x01D,0x01B,
	0x01A,0x018,0x017,0x016,0x014,0x013,0x012,0x011,0x010,0x00F,0x00E,0x00D};

const uint16_t PT3NoteTable_REAL_34_35[96] = {
	0xCDA,0xC22,0xB73,0xACF,0xA33,0x9A1,0x917,0x894,0x819,0x7A4,0x737,0x6CF,
	0x66D,0x611,0x5BA,0x567,0x51A,0x4D0,0x48B,0x44A,0x40C,0x3D2,0x39B,0x367,
	0x337,0x308,0x2DD,0x2B4,0x28D,0x268,0x246,0x225,0x206,0x1E9,0x1CE,0x1B4,
	0x19B,0x184,0x16E,0x15A,0x146,0x134,0x123,0x112,0x103,0x0F5,0x0E7,0x0DA,
	0x0CE,0x0C2,0x0B7,0x0AD,0x0A3,0x09A,0x091,0x089,0x082,0x07A,0x073,0x06D,
	0x067,0x061,0x05C,0x056,0x052,0x04D,0x049,0x045,0x041,0x03D,0x03A,0x036,
	0x033,0x031,0x02E,0x02B,0x029,0x027,0x024,0x022,0x020,0x01F,0x01D,0x01B,
	0x01A,0x018,0x017,0x016,0x014,0x013,0x012,0x011,0x010,0x00F,0x00E,0x00D};

const uint8_t PT3VolumeTable_33_34[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2},
	{0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3},
	{0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4},
	{0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5},
	{0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6},
	{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7},
	{0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8},
	{0, 0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 6, 7, 8, 8, 9},
	{0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 8, 8, 9,10},
	{0, 0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9,10,11},
	{0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 8, 9,11,11,12},
	{0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9,10,11,12,13},
	{0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15}};

const uint8_t PT3VolumeTable_35[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2},
	{0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3},
	{0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4},
	{0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5},
	{0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6},
	{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7},
	{0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8},
	{0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 7, 7, 8, 8, 9},
	{0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 7, 7, 8, 9, 9,10},
	{0, 1, 1, 2, 3, 4, 4, 5, 6, 7, 7, 8, 9,10,10,11},
	{0, 1, 2, 2, 3, 4, 5, 6, 6, 7, 8, 9,10,10,11,12},
	{0, 1, 2, 3, 3, 4, 5, 6, 7, 8, 9,10,10,11,12,13},
	{0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9,10,11,12,13,14},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15}};

typedef struct TPlConsts {
	const uint8_t *RAM_ptr;
	uint32_t Global_Tick_Counter;
	uint8_t  TS;
	uint8_t  Version;
	uint32_t Length;
    uint8_t  pt3_TonTableId;
    uint8_t  pt3_Delay;
    uint8_t  pt3_NumberOfPositions;
    uint8_t  pt3_LoopPosition;
    uint16_t pt3_PatternsPointer;
} TPlConsts;

typedef struct PT3_Channel_Parameters {
	uint16_t Address_In_Pattern;
	uint16_t OrnamentPointer;
	uint16_t SamplePointer;
	uint16_t Ton;
	uint8_t  Loop_Ornament_Position;
	uint8_t  Ornament_Length;
	uint8_t  Position_In_Ornament;
	uint8_t  Loop_Sample_Position;
	uint8_t  Sample_Length;
	uint8_t  Position_In_Sample;
	uint8_t  Volume;
	uint8_t  Number_Of_Notes_To_Skip;
	uint8_t  Note;
	uint8_t  Slide_To_Note;
	uint8_t  Amplitude;
	bool     Envelope_Enabled;
	bool     Enabled;
	bool     SimpleGliss;
	int16_t  Current_Amplitude_Sliding;
	int16_t  Current_Noise_Sliding;
	int16_t  Current_Envelope_Sliding;
	int16_t  Ton_Slide_Count;
	int16_t  Current_OnOff;
	int16_t  OnOff_Delay;
	int16_t  OffOn_Delay;
	int16_t  Ton_Slide_Delay;
	int16_t  Current_Ton_Sliding;
	int16_t  Ton_Accumulator;
	int16_t  Ton_Slide_Step;
	int16_t  Ton_Delta;
	int8_t   Note_Skip_Counter;
} PT3_Channel_Parameters;

typedef struct PT3_Parameters {
	union {
		uint16_t wrd;
		struct { uint8_t lo,hi; };
		} Env_Base;
	int16_t  Cur_Env_Slide;
	int16_t  Env_Slide_Add;
	int8_t   Cur_Env_Delay;
	int8_t   Env_Delay;
	uint8_t  Noise_Base;
	uint8_t  Delay;
	uint8_t  AddToNoise;
	uint8_t  DelayCounter;
	uint8_t  CurrentPosition;
} PT3_Parameters;

typedef struct TPlParams {
	PT3_Parameters PT3;
	PT3_Channel_Parameters PT3_[3]; //a+b+c
	uint8_t AY[14];
} TPlParams;

static TPlParams PlParams[1];
static TPlConsts PlConsts[1];
static uint8_t TempMixer;
static int16_t AddToEnv;

int forced_notetable = -1;

void func_mute(void) {
	for (int i = 0; i < 14; i++) {
		PlParams[0].AY[i] = 0;
	}
	PlParams[0].AY[7] = 0x3F; // Mixer: all off
}

void func_getregs(uint8_t *dest, int ch) {
    if (ch == 0)
	    memcpy(dest, PlParams[0].AY, 14);
}

static uint16_t GetNoteFreq(uint8_t j, int ch) {
    (void)ch;
	if (forced_notetable >= 0) {
		switch (forced_notetable) {
		    case 0: return PT3NoteTable_PT_33_34r[j];
		    case 1: return PT3NoteTable_PT_34_35[j];
		    case 2: return PT3NoteTable_ST[j];
		    case 3: return PT3NoteTable_ASM_34r[j];
		    case 4: return PT3NoteTable_ASM_34_35[j];
		    case 5: return PT3NoteTable_REAL_34r[j];
		    default: return PT3NoteTable_REAL_34_35[j];
		}
	} else {
		switch (PlConsts[0].pt3_TonTableId) {
		    case 0:
			    if (PlConsts[0].Version <= 3) return PT3NoteTable_PT_33_34r[j];
			    else return PT3NoteTable_PT_34_35[j];
		    case 1:
			    return PT3NoteTable_ST[j];
		    case 2:
			    if (PlConsts[0].Version <= 3) return PT3NoteTable_ASM_34r[j];
			    else return PT3NoteTable_ASM_34_35[j];
		    default:
			    if (PlConsts[0].Version <= 3) return PT3NoteTable_REAL_34r[j];
			    else return PT3NoteTable_REAL_34_35[j];
		}
	}
}

static void PatternInterpreter(PT3_Channel_Parameters *Chan, int ch) {
    (void)ch;
	const uint8_t *idx = PlConsts[0].RAM_ptr;
	TPlParams *pl = &PlParams[0];

	bool quit = false;
	uint8_t Flag1 = 0, Flag2 = 0, Flag3 = 0, Flag4 = 0, Flag5 = 0, Flag8 = 0, Flag9 = 0;
	uint8_t counter = 0, b;

	int PrNote = Chan->Note;
	int PrSliding = Chan->Current_Ton_Sliding;

	while (!quit) {
		uint8_t op = idx[Chan->Address_In_Pattern];
		if (op >= 0xf0) {
			Chan->OrnamentPointer = read_le16(idx + HDR_OrnamentsPointers + (op - 0xf0) * 2);
			Chan->Loop_Ornament_Position = idx[Chan->OrnamentPointer++];
			Chan->Ornament_Length = idx[Chan->OrnamentPointer++];
			Chan->Position_In_Ornament = 0;
			Chan->SamplePointer = read_le16(idx + HDR_SamplesPointers + (idx[++Chan->Address_In_Pattern]/2) * 2);
			Chan->Loop_Sample_Position = idx[Chan->SamplePointer++];
			Chan->Sample_Length = idx[Chan->SamplePointer++];
			Chan->Envelope_Enabled = false;
		} else if (op >= 0xd1) {
			Chan->SamplePointer = read_le16(idx + HDR_SamplesPointers + (op - 0xd0) * 2);
			Chan->Loop_Sample_Position = idx[Chan->SamplePointer++];
			Chan->Sample_Length = idx[Chan->SamplePointer++];
		} else if (op == 0xd0) {
			quit = true;
		} else if (op >= 0xc1) {
			Chan->Volume = op - 0xc0;
		} else if (op == 0xc0) {
			Chan->Position_In_Sample = 0;
			Chan->Current_Amplitude_Sliding = 0;
			Chan->Current_Noise_Sliding = 0;
			Chan->Current_Envelope_Sliding = 0;
			Chan->Position_In_Ornament = 0;
			Chan->Ton_Slide_Count = 0;
			Chan->Current_Ton_Sliding = 0;
			Chan->Ton_Accumulator = 0;
			Chan->Current_OnOff = 0;
			Chan->Enabled = false;
			quit = true;
		} else if (op >= 0xb2) {
			Chan->Envelope_Enabled = true;
			pl->AY[13] = op - 0xb1;
			pl->PT3.Env_Base.hi = idx[++Chan->Address_In_Pattern];
			pl->PT3.Env_Base.lo = idx[++Chan->Address_In_Pattern];
			Chan->Position_In_Ornament = 0;
			pl->PT3.Cur_Env_Slide = 0;
			pl->PT3.Cur_Env_Delay = 0;
		} else if (op == 0xb1) {
			Chan->Number_Of_Notes_To_Skip = idx[++Chan->Address_In_Pattern];
		} else if (op == 0xb0) {
			Chan->Envelope_Enabled = false;
			Chan->Position_In_Ornament = 0;
		} else if (op >= 0x50) {
			Chan->Note = op - 0x50;
			Chan->Position_In_Sample = 0;
			Chan->Current_Amplitude_Sliding = 0;
			Chan->Current_Noise_Sliding = 0;
			Chan->Current_Envelope_Sliding = 0;
			Chan->Position_In_Ornament = 0;
			Chan->Ton_Slide_Count = 0;
			Chan->Current_Ton_Sliding = 0;
			Chan->Ton_Accumulator = 0;
			Chan->Current_OnOff = 0;
			Chan->Enabled = true;
			quit = true;
		} else if (op >= 0x40) {
			Chan->OrnamentPointer = read_le16(idx + HDR_OrnamentsPointers + (op - 0x40) * 2);
			Chan->Loop_Ornament_Position = idx[Chan->OrnamentPointer++];
			Chan->Ornament_Length = idx[Chan->OrnamentPointer++];
			Chan->Position_In_Ornament = 0;
		} else if (op >= 0x20) {
			pl->PT3.Noise_Base = op - 0x20;
		} else if (op >= 0x11) {
			pl->AY[13] = op - 0x10;
			pl->PT3.Env_Base.hi = idx[++Chan->Address_In_Pattern];
			pl->PT3.Env_Base.lo = idx[++Chan->Address_In_Pattern];
			pl->PT3.Cur_Env_Slide = 0;
			pl->PT3.Cur_Env_Delay = 0;
			Chan->Envelope_Enabled = true;
			Chan->SamplePointer = read_le16(idx + HDR_SamplesPointers + (idx[++Chan->Address_In_Pattern]/2) * 2);
			Chan->Loop_Sample_Position = idx[Chan->SamplePointer++];
			Chan->Sample_Length = idx[Chan->SamplePointer++];
			Chan->Position_In_Ornament = 0;
		} else if (op == 0x10) {
			Chan->Envelope_Enabled = false;
			Chan->SamplePointer = read_le16(idx + HDR_SamplesPointers + (idx[++Chan->Address_In_Pattern]/2) * 2);
			Chan->Loop_Sample_Position = idx[Chan->SamplePointer++];
			Chan->Sample_Length = idx[Chan->SamplePointer++];
			Chan->Position_In_Ornament = 0;
		} else if (op == 9) Flag9 = ++counter;
		else if (op == 8) Flag8 = ++counter;
		else if (op == 5) Flag5 = ++counter;
		else if (op == 4) Flag4 = ++counter;
		else if (op == 3) Flag3 = ++counter;
		else if (op == 2) Flag2 = ++counter;
		else if (op == 1) Flag1 = ++counter;
		Chan->Address_In_Pattern++;
	}
    while (counter > 0){
		if (counter == Flag1) {
			Chan->Ton_Slide_Delay = idx[Chan->Address_In_Pattern++];
			Chan->Ton_Slide_Count = Chan->Ton_Slide_Delay;
			Chan->Ton_Slide_Step = (int16_t)read_le16(idx + Chan->Address_In_Pattern);
			Chan->SimpleGliss = true;
			Chan->Current_OnOff = 0;
			if ((Chan->Ton_Slide_Count == 0) && (PlConsts[0].Version >= 7))
				Chan->Ton_Slide_Count++;
			Chan->Address_In_Pattern += 2;
		} else if (counter == Flag2) {
			Chan->SimpleGliss = false;
			Chan->Current_OnOff = 0;
			Chan->Ton_Slide_Delay = idx[Chan->Address_In_Pattern++];
			Chan->Ton_Slide_Count = Chan->Ton_Slide_Delay;
			Chan->Ton_Slide_Step = abs((int16_t)read_le16(idx + Chan->Address_In_Pattern + 2));
			Chan->Ton_Delta = GetNoteFreq(Chan->Note, 0) - GetNoteFreq(PrNote, 0);
			Chan->Slide_To_Note = Chan->Note;
			Chan->Note = PrNote;
			if (PlConsts[0].Version >= 6)
				Chan->Current_Ton_Sliding = PrSliding;
			if (Chan->Ton_Delta - Chan->Current_Ton_Sliding < 0)
				Chan->Ton_Slide_Step = -Chan->Ton_Slide_Step;
			Chan->Address_In_Pattern += 4;
		} else if (counter == Flag3) {
	        Chan->Position_In_Sample = idx[Chan->Address_In_Pattern++];
		} else if (counter == Flag4) {
	        Chan->Position_In_Ornament = idx[Chan->Address_In_Pattern++];
		} else if (counter == Flag5) {
			Chan->OnOff_Delay = idx[Chan->Address_In_Pattern++];
			Chan->OffOn_Delay = idx[Chan->Address_In_Pattern++];
			Chan->Current_OnOff = Chan->OnOff_Delay;
			Chan->Ton_Slide_Count = 0;
			Chan->Current_Ton_Sliding = 0;
		} else if (counter == Flag8) {
			pl->PT3.Env_Delay = idx[Chan->Address_In_Pattern++];
			pl->PT3.Cur_Env_Delay = pl->PT3.Env_Delay;
			pl->PT3.Env_Slide_Add = (int16_t)read_le16(idx + Chan->Address_In_Pattern);
			Chan->Address_In_Pattern += 2;
		} else if (counter == Flag9) {
			b = idx[Chan->Address_In_Pattern++];
			pl->PT3.Delay = b;
		}
		counter--;
	}
	Chan->Note_Skip_Counter = Chan->Number_Of_Notes_To_Skip;
}

static void ChangeRegisters(PT3_Channel_Parameters *Chan, int ch) {
    (void)ch;
	uint8_t j, b0, b1;
	int16_t w;
	const uint8_t *idx = PlConsts[0].RAM_ptr;

	if (Chan->Enabled) {
		Chan->Ton = read_le16(idx + Chan->SamplePointer + Chan->Position_In_Sample*4 + 2);
		Chan->Ton += Chan->Ton_Accumulator;
		b0 = idx[Chan->SamplePointer + Chan->Position_In_Sample*4 + 0];
		b1 = idx[Chan->SamplePointer + Chan->Position_In_Sample*4 + 1];
		if (b1 & 0x40) Chan->Ton_Accumulator = Chan->Ton;
		j = Chan->Note + idx[Chan->OrnamentPointer + Chan->Position_In_Ornament];
		if (j >= 128) j = 0; else if (j > 95) j = 95;
		w = GetNoteFreq(j, 0);
		Chan->Ton = (Chan->Ton + Chan->Current_Ton_Sliding + w) & 0xfff;
		if (Chan->Ton_Slide_Count > 0) {
			Chan->Ton_Slide_Count--;
			if (Chan->Ton_Slide_Count == 0) {
				Chan->Current_Ton_Sliding += Chan->Ton_Slide_Step;
				Chan->Ton_Slide_Count = Chan->Ton_Slide_Delay;
				if (!Chan->SimpleGliss) {
					if (((Chan->Ton_Slide_Step < 0) && (Chan->Current_Ton_Sliding <= Chan->Ton_Delta)) ||
					    ((Chan->Ton_Slide_Step >= 0) && (Chan->Current_Ton_Sliding >= Chan->Ton_Delta))) {
						Chan->Note = Chan->Slide_To_Note;
						Chan->Ton_Slide_Count = 0;
						Chan->Current_Ton_Sliding = 0;
					}
				}
			}
		}
		Chan->Amplitude = b1 & 15;
		if (b0 & 0x80) {
			if (b0 & 0x40) { if (Chan->Current_Amplitude_Sliding < 15) Chan->Current_Amplitude_Sliding++; }
			else { if (Chan->Current_Amplitude_Sliding > -15) Chan->Current_Amplitude_Sliding--; }
		}
		Chan->Amplitude += Chan->Current_Amplitude_Sliding;
		if (Chan->Amplitude >= 128) Chan->Amplitude = 0;
		else if (Chan->Amplitude > 15) Chan->Amplitude = 15;

		if (PlConsts[0].Version <= 4) Chan->Amplitude = PT3VolumeTable_33_34[Chan->Volume][Chan->Amplitude];
		else Chan->Amplitude = PT3VolumeTable_35[Chan->Volume][Chan->Amplitude];

		if (!(b0 & 1) && Chan->Envelope_Enabled) Chan->Amplitude = Chan->Amplitude | 0x10;
		if (b1 & 0x80) {
            uint8_t env_val;
			if (b0 & 0x20) env_val = (uint8_t)((b0>>1) | (0xF0 + Chan->Current_Envelope_Sliding));
			else env_val = (uint8_t)((b0>>1) & (0x0f + Chan->Current_Envelope_Sliding));
			if (b1 & 0x20) Chan->Current_Envelope_Sliding = (int8_t)env_val;
			AddToEnv += env_val;
		} else {
			PlParams[0].PT3.AddToNoise = (b0>>1) + Chan->Current_Noise_Sliding;
			if (b1 & 0x20) Chan->Current_Noise_Sliding = PlParams[0].PT3.AddToNoise;
		}
		TempMixer = ((b1>>1) & 0x48) | TempMixer;
		Chan->Position_In_Sample++;
		if (Chan->Position_In_Sample >= Chan->Sample_Length) Chan->Position_In_Sample = Chan->Loop_Sample_Position;
		Chan->Position_In_Ornament++;
		if (Chan->Position_In_Ornament >= Chan->Ornament_Length) Chan->Position_In_Ornament = Chan->Loop_Ornament_Position;
	} else {
    	Chan->Amplitude = 0;
    	TempMixer |= 0x48;  // disable both tone and noise for this channel's position
	}

	TempMixer >>= 1;
	if (Chan->Current_OnOff > 0) {
		Chan->Current_OnOff--;
		if (Chan->Current_OnOff == 0) {
			Chan->Enabled = !Chan->Enabled;
			if (Chan->Enabled) Chan->Current_OnOff = Chan->OnOff_Delay;
			else Chan->Current_OnOff = Chan->OffOn_Delay;
		}
	}
}

void func_play_tick(int ch) {
    if (ch != 0) return;
	int track_idx;
	const uint8_t *idx = PlConsts[0].RAM_ptr;
	TPlParams *pl = &PlParams[0];
	PT3_Parameters *pt3 = &PlParams[0].PT3;

	pl->AY[13] = 0xff;
	pt3->DelayCounter--;
	if (pt3->DelayCounter == 0) {
		pl->PT3_[0].Note_Skip_Counter--;
		if (pl->PT3_[0].Note_Skip_Counter == 0) {
			if (idx[pl->PT3_[0].Address_In_Pattern] == 0) {
				pt3->CurrentPosition++;
				if (pt3->CurrentPosition == PlConsts[0].pt3_NumberOfPositions)
					pt3->CurrentPosition = PlConsts[0].pt3_LoopPosition;
				track_idx = idx[HDR_PositionList + pt3->CurrentPosition];
				for (int abc = 0; abc < 3; abc++)
					pl->PT3_[abc].Address_In_Pattern = read_le16(idx + PlConsts[0].pt3_PatternsPointer + (track_idx + abc) * 2);
				pt3->Noise_Base = 0;
			}
			PatternInterpreter(&pl->PT3_[0], 0);
		}
		for (int abc = 1; abc < 3; abc++) {
			pl->PT3_[abc].Note_Skip_Counter--;
			if (pl->PT3_[abc].Note_Skip_Counter == 0) 
				PatternInterpreter(&pl->PT3_[abc], 0);
		}
		pt3->DelayCounter = pt3->Delay;
	}
	AddToEnv = 0;
	TempMixer = 0;
	ChangeRegisters(&pl->PT3_[0], 0);
	ChangeRegisters(&pl->PT3_[1], 0);
	ChangeRegisters(&pl->PT3_[2], 0);

	pl->AY[0] = pl->PT3_[0].Ton & 0xff;
	pl->AY[1] = pl->PT3_[0].Ton >> 8;
	pl->AY[2] = pl->PT3_[1].Ton & 0xff;
	pl->AY[3] = pl->PT3_[1].Ton >> 8;
	pl->AY[4] = pl->PT3_[2].Ton & 0xff;
	pl->AY[5] = pl->PT3_[2].Ton >> 8;
	pl->AY[6] = (pt3->Noise_Base + pt3->AddToNoise) & 0x1f;
	pl->AY[7] = TempMixer;
	pl->AY[8] = pl->PT3_[0].Amplitude;
	pl->AY[9] = pl->PT3_[1].Amplitude;
	pl->AY[10] = pl->PT3_[2].Amplitude;
	uint16_t env = pt3->Env_Base.wrd + AddToEnv + pt3->Cur_Env_Slide;
	pl->AY[11] = env & 0xff;
	pl->AY[12] = env >> 8;

	if (pt3->Cur_Env_Delay > 0) {
		pt3->Cur_Env_Delay--;
		if (pt3->Cur_Env_Delay == 0) {
			pt3->Cur_Env_Delay = pt3->Env_Delay;
			pt3->Cur_Env_Slide += pt3->Env_Slide_Add;
		}
	}
	PlConsts[0].Global_Tick_Counter++;
}

int func_restart_music(int ch) {
    if (ch != 0) return 0;
	int track_idx;
	const uint8_t *idx = PlConsts[0].RAM_ptr;
	TPlParams *pl = &PlParams[0];
	PT3_Parameters *pt3 = &PlParams[0].PT3;

	pt3->DelayCounter = 1;
	pt3->Delay = PlConsts[0].pt3_Delay;
	pt3->Noise_Base = 0;
	pt3->AddToNoise = 0;
	pt3->Cur_Env_Slide = 0;
	pt3->Cur_Env_Delay = 0;
	pt3->Env_Base.wrd = 0;
	pt3->CurrentPosition = 0;

	track_idx = idx[HDR_PositionList + 0];
	for (int abc = 0; abc < 3; abc++) {
		pl->PT3_[abc].Address_In_Pattern = read_le16(idx + PlConsts[0].pt3_PatternsPointer + (track_idx + abc) * 2);
		pl->PT3_[abc].OrnamentPointer = read_le16(idx + HDR_OrnamentsPointers + 0 * 2);
		pl->PT3_[abc].Loop_Ornament_Position = idx[pl->PT3_[abc].OrnamentPointer++];
		pl->PT3_[abc].Ornament_Length = idx[pl->PT3_[abc].OrnamentPointer++];
		pl->PT3_[abc].SamplePointer = read_le16(idx + HDR_SamplesPointers + 1 * 2);
		pl->PT3_[abc].Loop_Sample_Position = idx[pl->PT3_[abc].SamplePointer++];
		pl->PT3_[abc].Sample_Length = idx[pl->PT3_[abc].SamplePointer++];
		pl->PT3_[abc].Volume = 15;
		pl->PT3_[abc].Note_Skip_Counter = 1;
		pl->PT3_[abc].Enabled = false;
		pl->PT3_[abc].Envelope_Enabled = false;
		pl->PT3_[abc].Note = 0;
		pl->PT3_[abc].Ton = 0;
	}
    return 1;
}

int func_setup_music(const uint8_t* music_ptr, int length, int chn, int first) {
    if (chn != 0) return 0;
    (void)length; (void)first;
    PlConsts[0].RAM_ptr = music_ptr;
	PlConsts[0].Version = 6;
	if (music_ptr[13] >= '0' && music_ptr[13] <= '9') {
		PlConsts[0].Version = music_ptr[13] - '0';
	}
	PlConsts[0].Global_Tick_Counter = 0;
    PlConsts[0].pt3_TonTableId = music_ptr[HDR_TonTableId];
    PlConsts[0].pt3_Delay = music_ptr[HDR_Delay];
    PlConsts[0].pt3_NumberOfPositions = music_ptr[HDR_NumberOfPositions];
    PlConsts[0].pt3_LoopPosition = music_ptr[HDR_LoopPosition];
    PlConsts[0].pt3_PatternsPointer = read_le16(music_ptr + HDR_PatternsPointer);

    func_restart_music(0);
	return 1;
}
